#include "cockpit/media/media_service.hpp"

#include "cockpit/media/nv12_rgb.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace cockpit::media {

MediaService::MediaService(MediaServiceConfig config,
                           std::unique_ptr<ICameraCapture> capture,
                           std::shared_ptr<PreviewMailbox> mailbox)
    : config_(std::move(config)), capture_(std::move(capture)), mailbox_(std::move(mailbox)) {}

MediaService::~MediaService() { stop(); }

MediaStatus MediaService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return MediaStatus::Ok("already running");
    if (!capture_ || !mailbox_ || config_.capture.device.empty() ||
        config_.snapshot_directory.empty() || config_.operation_capacity == 0)
        return {MediaStatusCode::InvalidArgument, "media service configuration"};
    stopping_ = false;
    running_ = true;
    mailbox_->reset();
    try {
        worker_ = std::thread(&MediaService::run, this);
    } catch (...) {
        running_ = false;
        return {MediaStatusCode::Unavailable, "media service worker start"};
    }
    return MediaStatus::Ok();
}

void MediaService::stop() {
    std::deque<Request> cancelled;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && !worker_.joinable()) return;
        stopping_ = true;
        cancelled.swap(requests_);
    }
    for (auto& request : cancelled) {
        if (request.completion)
            request.completion({{MediaStatusCode::Cancelled, "media service stopping"}, {}, {}});
    }
    work_ready_.notify_all();
    frame_ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    mailbox_->stop();
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    streaming_ = false;
    latest_frame_.reset();
}

MediaStatus MediaService::submit(MediaOperation operation, MediaOperationCompletion completion) {
    if (!completion) return {MediaStatusCode::InvalidArgument, "operation completion"};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopping_)
            return {MediaStatusCode::InvalidState, "media service is stopped"};
        if (requests_.size() >= config_.operation_capacity)
            return {MediaStatusCode::Unavailable, "media operation queue full"};
        requests_.push_back({operation, std::move(completion)});
    }
    work_ready_.notify_one();
    return MediaStatus::Ok("accepted");
}

bool MediaService::streaming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

CameraFormat MediaService::actual_format() const { return capture_->actual_format(); }

CaptureStats MediaService::capture_stats() const { return capture_->stats(); }

MediaServiceStats MediaService::service_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::shared_ptr<const CapturedFrame> MediaService::latest_frame() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_frame_;
}

void MediaService::run() {
    for (;;) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_ready_.wait(lock, [&] { return stopping_ || !requests_.empty(); });
            if (stopping_) break;
            request = std::move(requests_.front());
            requests_.pop_front();
        }
        auto result = execute(request.operation);
        if (!result.status.ok()) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.operation_failures;
        }
        if (request.completion) request.completion(std::move(result));
    }
    if (capture_->streaming()) (void)capture_->stop();
    capture_->close_device();
}

MediaOperationResult MediaService::execute(MediaOperation operation) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (operation) {
        case MediaOperation::PreviewStart: ++stats_.preview_start_requests; break;
        case MediaOperation::PreviewStop: ++stats_.preview_stop_requests; break;
        case MediaOperation::Snapshot: ++stats_.snapshot_requests; break;
        }
    }
    switch (operation) {
    case MediaOperation::PreviewStart: return start_preview();
    case MediaOperation::PreviewStop: return stop_preview();
    case MediaOperation::Snapshot: return snapshot();
    }
    return {{MediaStatusCode::InvalidArgument, "unknown media operation"}, {}, {}};
}

MediaOperationResult MediaService::start_preview() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (streaming_) return {MediaStatus::Ok("preview already streaming"), latest_frame_, {}};
    }
    auto status = capture_->open_device(config_.capture.device);
    if (!status.ok()) return {std::move(status), {}, {}};
    status = capture_->configure(config_.capture);
    if (!status.ok()) {
        capture_->close_device();
        return {std::move(status), {}, {}};
    }
    mailbox_->reset();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_frame_.reset();
    }
    status = capture_->start([this](CapturedFrame frame) { receive_frame(std::move(frame)); });
    if (!status.ok()) {
        capture_->close_device();
        return {std::move(status), {}, {}};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        streaming_ = true;
    }
    const auto format = capture_->actual_format();
    std::ostringstream detail;
    detail << "preview streaming " << format.width << 'x' << format.height << ' '
           << format.pixel_format << " planes=" << format.num_planes
           << " buffers=" << format.actual_buffers
           << " epoch=" << capture_->stats().stream_epoch;
    return {MediaStatus::Ok(detail.str()), {}, {}};
}

MediaOperationResult MediaService::stop_preview() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!streaming_) return {MediaStatus::Ok("preview already stopped"), {}, {}};
    }
    auto status = capture_->stop();
    const auto stats = capture_->stats();
    capture_->close_device();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        streaming_ = false;
        latest_frame_.reset();
    }
    if (!status.ok()) return {std::move(status), {}, {}};
    std::ostringstream detail;
    detail << "preview stopped epoch=" << stats.stream_epoch << " frames=" << stats.frames;
    return {MediaStatus::Ok(detail.str()), {}, {}};
}

MediaOperationResult MediaService::snapshot() {
    std::shared_ptr<const CapturedFrame> frame;
    std::uint64_t version = 0;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!streaming_)
            return {{MediaStatusCode::CameraNotStreaming, "CAMERA_NOT_STREAMING"}, {}, {}};
        version = latest_frame_version_;
        if (!frame_ready_.wait_for(lock, config_.snapshot_wait, [&] {
                return stopping_ || latest_frame_version_ > version;
            }))
            return {{MediaStatusCode::Timeout, "snapshot frame timeout"}, {}, {}};
        if (stopping_ || !latest_frame_)
            return {{MediaStatusCode::Cancelled, "snapshot cancelled"}, {}, {}};
        frame = latest_frame_;
    }

    std::vector<std::uint8_t> rgb;
    auto status = nv12_to_rgb888(*frame, rgb);
    if (!status.ok()) return {std::move(status), frame, {}};
    std::error_code error;
    std::filesystem::create_directories(config_.snapshot_directory, error);
    if (error)
        return {{MediaStatusCode::IoError, "create snapshot directory: " + error.message()},
                frame, {}};
    const auto path = std::filesystem::path(config_.snapshot_directory) /
                      ("cam0_e" + std::to_string(frame->stream_epoch) + "_s" +
                       std::to_string(frame->sequence) + ".ppm");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return {{MediaStatusCode::IoError, "open snapshot output"}, frame, path.string()};
    output << "P6\n" << frame->width << ' ' << frame->height << "\n255\n";
    output.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    output.close();
    if (!output)
        return {{MediaStatusCode::IoError, "write snapshot output"}, frame, path.string()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.snapshots_written;
    }
    std::ostringstream detail;
    detail << "snapshot camera=" << frame->camera_id << " epoch=" << frame->stream_epoch
           << " sequence=" << frame->sequence << " timestamp_ns="
           << frame->capture_timestamp_ns << " path=" << path.string();
    return {MediaStatus::Ok(detail.str()), frame, path.string()};
}

void MediaService::receive_frame(CapturedFrame frame) {
    auto owned = std::make_shared<CapturedFrame>(std::move(frame));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        latest_frame_ = owned;
        ++latest_frame_version_;
        ++stats_.preview_frames_published;
    }
    mailbox_->publish(std::move(owned));
    frame_ready_.notify_all();
}

}  // namespace cockpit::media
