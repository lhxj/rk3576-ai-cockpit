#include "cockpit_ui/qt_preview_bridge.h"

#include "cockpit/media/nv12_rgb.hpp"

#include <QMetaObject>

#include <chrono>
#include <utility>
#include <vector>

namespace cockpit::ui {

QtPreviewBridge::QtPreviewBridge(std::shared_ptr<media::PreviewMailbox> mailbox)
    : mailbox_(std::move(mailbox)) {}

QtPreviewBridge::~QtPreviewBridge() { stop(); }

bool QtPreviewBridge::start(FrameCallback callback) {
    if (!mailbox_ || !callback || worker_.joinable()) return false;
    callback_ = std::move(callback);
    stopping_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = {};
        pending_generation_ = 0;
        delivered_generation_ = 0;
        ui_drops_ = 0;
        converted_frames_ = 0;
        delivered_frames_ = 0;
        throttled_frames_ = 0;
        displayed_fps_ = 0.0;
        first_delivery_ = {};
    }
    try {
        worker_ = std::thread(&QtPreviewBridge::run, this);
    } catch (...) {
        callback_ = {};
        return false;
    }
    return true;
}

void QtPreviewBridge::stop() {
    stopping_.store(true);
    if (worker_.joinable()) worker_.join();
    callback_ = {};
}

QtPreviewStats QtPreviewBridge::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {converted_frames_, delivered_frames_, throttled_frames_, ui_drops_,
            mailbox_ ? mailbox_->drop_count() : 0, displayed_fps_};
}

void QtPreviewBridge::run() {
    media::PreviewEpochFilter epoch_filter;
    std::uint64_t delivery_id = 0;
    auto last_conversion = std::chrono::steady_clock::time_point{};
    constexpr auto minimum_interval = std::chrono::milliseconds(66);

    while (!stopping_.load()) {
        media::PreviewDelivery delivery;
        if (!mailbox_->wait_next(delivery_id, std::chrono::milliseconds(100), delivery)) continue;
        delivery_id = delivery.delivery_id;
        if (!delivery.frame || !epoch_filter.accept(*delivery.frame)) continue;
        const auto now = std::chrono::steady_clock::now();
        if (last_conversion != std::chrono::steady_clock::time_point{} &&
            now - last_conversion < minimum_interval) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++throttled_frames_;
            continue;
        }
        last_conversion = now;

        std::vector<std::uint8_t> rgb;
        if (!media::nv12_to_rgb888(*delivery.frame, rgb).ok()) continue;
        QImage wrapped(rgb.data(), static_cast<int>(delivery.frame->width),
                       static_cast<int>(delivery.frame->height),
                       static_cast<int>(delivery.frame->width * 3U), QImage::Format_RGB888);
        if (wrapped.isNull()) continue;
        PreviewFrameMetadata metadata{delivery.frame->camera_id,
                                      delivery.frame->width,
                                      delivery.frame->height,
                                      delivery.frame->pixel_format,
                                      delivery.frame->sequence,
                                      delivery.frame->stream_epoch,
                                      delivery.frame->capture_timestamp_ns};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_generation_ > delivered_generation_) ++ui_drops_;
            ++converted_frames_;
            pending_ = {wrapped.copy(), std::move(metadata), 0.0, mailbox_->drop_count(),
                        ui_drops_ + throttled_frames_};
            ++pending_generation_;
        }
        if (!delivery_queued_.exchange(true)) {
            QMetaObject::invokeMethod(this, [this] { deliver_latest(); }, Qt::QueuedConnection);
        }
    }
}

void QtPreviewBridge::deliver_latest() {
    PendingImage image;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        image = pending_;
        delivered_generation_ = pending_generation_;
        delivery_queued_.store(false);
        const auto now = std::chrono::steady_clock::now();
        if (first_delivery_ == std::chrono::steady_clock::time_point{}) first_delivery_ = now;
        ++delivered_frames_;
        const auto elapsed = std::chrono::duration<double>(now - first_delivery_).count();
        displayed_fps_ = elapsed > 0.0 && delivered_frames_ > 1
                             ? static_cast<double>(delivered_frames_ - 1) / elapsed
                             : 0.0;
        image.preview_fps = displayed_fps_;
    }
    if (callback_ && !image.image.isNull())
        callback_(std::move(image.image), std::move(image.metadata), image.preview_fps,
                  image.mailbox_drops, image.ui_drops);
}

}  // namespace cockpit::ui
