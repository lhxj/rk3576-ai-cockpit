#include "cockpit/media/preview_mailbox.hpp"

namespace cockpit::media {

void PreviewMailbox::publish(std::shared_ptr<const CapturedFrame> frame) {
    if (!frame) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        if (latest_ && latest_id_ > delivered_id_) ++drops_;
        latest_ = std::move(frame);
        ++latest_id_;
    }
    ready_.notify_one();
}

bool PreviewMailbox::wait_next(std::uint64_t last_delivery_id,
                               std::chrono::milliseconds timeout,
                               PreviewDelivery& delivery) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!ready_.wait_for(lock, timeout,
                         [&] { return stopped_ || (latest_ && latest_id_ > last_delivery_id); }))
        return false;
    if (stopped_ || !latest_) return false;
    delivery = {latest_id_, latest_};
    delivered_id_ = latest_id_;
    return true;
}

void PreviewMailbox::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    ready_.notify_all();
}

void PreviewMailbox::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_.reset();
    delivered_id_ = latest_id_;
    stopped_ = false;
}

std::uint64_t PreviewMailbox::drop_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return drops_;
}

bool PreviewEpochFilter::accept(const CapturedFrame& frame) {
    if (frame.stream_epoch == 0) {
        ++stale_drops_;
        return false;
    }
    if (frame.stream_epoch < epoch_ ||
        (frame.stream_epoch == epoch_ && frame.sequence <= sequence_ && epoch_ != 0)) {
        ++stale_drops_;
        return false;
    }
    if (frame.stream_epoch > epoch_) {
        epoch_ = frame.stream_epoch;
        sequence_ = frame.sequence;
        return true;
    }
    sequence_ = frame.sequence;
    return true;
}

}  // namespace cockpit::media
