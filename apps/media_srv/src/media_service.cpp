#include "cockpit/media/media_service.hpp"

#include "cockpit/media/nv12_rgb.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace cockpit::media {

MediaService::MediaService(MediaServiceConfig config,
                           std::unique_ptr<ICameraCapture> capture,
                           std::shared_ptr<PreviewMailbox> mailbox,
                           std::unique_ptr<IMediaRecorder> recorder)
    : config_(std::move(config)), capture_(std::move(capture)),
      recorder_(std::move(recorder)), mailbox_(std::move(mailbox)) {}

MediaService::~MediaService() { stop(); }

MediaStatus MediaService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return MediaStatus::Ok("already running");
    if (!capture_ || !mailbox_ || config_.capture.device.empty() ||
        config_.snapshot_directory.empty() || config_.operation_capacity == 0 ||
        (recorder_ && (config_.recording_directory.empty() ||
                       config_.recorder.queue_capacity == 0)))
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
        recording_accepting_ = false;
        cancelled.swap(requests_);
    }
    for (auto& request : cancelled) {
        if (request.completion)
            request.completion({{MediaStatusCode::Cancelled, "media service stopping"},
                                {}, {}, {}});
    }
    work_ready_.notify_all();
    frame_ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    mailbox_->stop();
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    capture_prepared_ = false;
    capture_active_ = false;
    preview_active_ = false;
    recording_accepting_ = false;
    recording_active_ = false;
    latest_frame_.reset();
}

MediaStatus MediaService::submit(MediaOperation operation, MediaOperationCompletion completion) {
    if (!completion || operation == MediaOperation::RecordingFailure)
        return {MediaStatusCode::InvalidArgument, "operation completion"};
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
    return capture_active_;
}

bool MediaService::preview_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return preview_active_;
}

bool MediaService::recording_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return recording_active_;
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

RecorderStats MediaService::recorder_stats() const {
    return recorder_ ? recorder_->stats() : RecorderStats{};
}

void MediaService::set_recording_failure_callback(RecordingFailureCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    recording_failure_callback_ = std::move(callback);
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
        if (!result.status.ok() && request.operation != MediaOperation::RecordingFailure) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.operation_failures;
        }
        if (request.completion) request.completion(std::move(result));
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = false;
    }
    if (recorder_) (void)recorder_->stop();
    (void)stop_capture();
}

MediaOperationResult MediaService::execute(MediaOperation operation) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (operation) {
        case MediaOperation::PreviewStart: ++stats_.preview_start_requests; break;
        case MediaOperation::PreviewStop: ++stats_.preview_stop_requests; break;
        case MediaOperation::Snapshot: ++stats_.snapshot_requests; break;
        case MediaOperation::RecordingStart: ++stats_.recording_start_requests; break;
        case MediaOperation::RecordingStop: ++stats_.recording_stop_requests; break;
        case MediaOperation::RecordingFailure: break;
        }
    }
    switch (operation) {
    case MediaOperation::PreviewStart: return start_preview();
    case MediaOperation::PreviewStop: return stop_preview();
    case MediaOperation::Snapshot: return snapshot();
    case MediaOperation::RecordingStart: return start_recording();
    case MediaOperation::RecordingStop: return stop_recording();
    case MediaOperation::RecordingFailure: {
        MediaStatus status;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status = recording_failure_status_;
        }
        handle_recording_failure(status);
        return {std::move(status), {}, {}, recorder_stats()};
    }
    }
    return {{MediaStatusCode::InvalidArgument, "unknown media operation"}, {}, {}, {}};
}

MediaStatus MediaService::prepare_capture() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capture_prepared_) return MediaStatus::Ok();
    }
    auto status = capture_->open_device(config_.capture.device);
    if (!status.ok()) return status;
    status = capture_->configure(config_.capture);
    if (!status.ok()) {
        capture_->close_device();
        return status;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    capture_prepared_ = true;
    return MediaStatus::Ok();
}

MediaStatus MediaService::start_capture() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (capture_active_) return MediaStatus::Ok("capture already streaming");
    }
    auto status = prepare_capture();
    if (!status.ok()) return status;
    status = capture_->start([this](CapturedFrame frame) { receive_frame(std::move(frame)); });
    if (!status.ok()) {
        capture_->close_device();
        std::lock_guard<std::mutex> lock(mutex_);
        capture_prepared_ = false;
        return status;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    capture_active_ = true;
    return MediaStatus::Ok();
}

MediaStatus MediaService::stop_capture_if_unused() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (preview_active_ || recording_accepting_ || recording_active_)
            return MediaStatus::Ok("capture retained by consumer");
    }
    return stop_capture();
}

MediaStatus MediaService::stop_capture() {
    bool was_active = false;
    bool was_prepared = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        was_active = capture_active_;
        was_prepared = capture_prepared_;
    }
    MediaStatus status = MediaStatus::Ok("capture already stopped");
    if (was_active) status = capture_->stop();
    if (was_prepared) capture_->close_device();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capture_active_ = false;
        capture_prepared_ = false;
        latest_frame_.reset();
    }
    return status;
}

MediaOperationResult MediaService::start_preview() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (preview_active_)
            return {MediaStatus::Ok("preview already streaming"), latest_frame_, {}, {}};
    }
    mailbox_->reset();
    auto status = start_capture();
    if (!status.ok()) return {std::move(status), {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        preview_active_ = true;
    }
    const auto format = capture_->actual_format();
    std::ostringstream detail;
    detail << "preview streaming " << format.width << 'x' << format.height << ' '
           << format.pixel_format << " planes=" << format.num_planes
           << " buffers=" << format.actual_buffers
           << " epoch=" << capture_->stats().stream_epoch;
    return {MediaStatus::Ok(detail.str()), {}, {}, {}};
}

MediaOperationResult MediaService::stop_preview() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!preview_active_)
            return {MediaStatus::Ok("preview already stopped"), {}, {}, {}};
        preview_active_ = false;
    }
    mailbox_->reset();
    const auto stats = capture_->stats();
    auto status = stop_capture_if_unused();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_frame_.reset();
    }
    if (!status.ok()) return {std::move(status), {}, {}, {}};
    std::ostringstream detail;
    detail << "preview stopped epoch=" << stats.stream_epoch << " frames=" << stats.frames;
    return {MediaStatus::Ok(detail.str()), {}, {}, {}};
}

MediaOperationResult MediaService::snapshot() {
    std::shared_ptr<const CapturedFrame> frame;
    std::uint64_t version = 0;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!capture_active_)
            return {{MediaStatusCode::CameraNotStreaming, "CAMERA_NOT_STREAMING"}, {}, {}, {}};
        version = latest_frame_version_;
        if (!frame_ready_.wait_for(lock, config_.snapshot_wait, [&] {
                return stopping_ || latest_frame_version_ > version;
            }))
            return {{MediaStatusCode::Timeout, "snapshot frame timeout"}, {}, {}, {}};
        if (stopping_ || !latest_frame_)
            return {{MediaStatusCode::Cancelled, "snapshot cancelled"}, {}, {}, {}};
        frame = latest_frame_;
    }
    std::vector<std::uint8_t> rgb;
    auto status = nv12_to_rgb888(*frame, rgb);
    if (!status.ok()) return {std::move(status), frame, {}, {}};
    std::error_code error;
    std::filesystem::create_directories(config_.snapshot_directory, error);
    if (error)
        return {{MediaStatusCode::IoError, "create snapshot directory: " + error.message()},
                frame, {}, {}};
    const auto path = std::filesystem::path(config_.snapshot_directory) /
                      ("cam0_e" + std::to_string(frame->stream_epoch) + "_s" +
                       std::to_string(frame->sequence) + ".ppm");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return {{MediaStatusCode::IoError, "open snapshot output"}, frame, path.string(), {}};
    output << "P6\n" << frame->width << ' ' << frame->height << "\n255\n";
    output.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    output.close();
    if (!output)
        return {{MediaStatusCode::IoError, "write snapshot output"}, frame, path.string(), {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.snapshots_written;
    }
    std::ostringstream detail;
    detail << "snapshot camera=" << frame->camera_id << " epoch=" << frame->stream_epoch
           << " sequence=" << frame->sequence << " timestamp_ns="
           << frame->capture_timestamp_ns << " path=" << path.string();
    return {MediaStatus::Ok(detail.str()), frame, path.string(), {}};
}

MediaOperationResult MediaService::start_recording() {
    if (!recorder_)
        return {{MediaStatusCode::NotImplemented, "recording backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (recording_active_ || recording_accepting_) {
            const auto stats = recorder_->stats();
            return {MediaStatus::Ok("recording already active"), {}, stats.output_path, stats};
        }
        recording_failure_queued_ = false;
        recording_failure_status_ = MediaStatus::Ok();
    }
    std::error_code error;
    std::filesystem::create_directories(config_.recording_directory, error);
    if (error)
        return {{MediaStatusCode::IoError, "create recording directory: " + error.message()},
                {}, {}, {}};
    auto status = prepare_capture();
    if (!status.ok()) return {std::move(status), {}, {}, {}};
    const auto id = ++next_recording_id_;
    const auto path = (std::filesystem::path(config_.recording_directory) /
                       ("recording_" + std::to_string(id) + ".h264")).string();
    status = recorder_->start(config_.recorder, capture_->actual_format(), path);
    if (!status.ok()) {
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, recorder_->stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = true;
    }
    status = start_capture();
    if (!status.ok()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recording_accepting_ = false;
        }
        (void)recorder_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, recorder_->stats()};
    }
    status = recorder_->wait_for_first_packet(config_.recorder.first_packet_timeout);
    if (!status.ok()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recording_accepting_ = false;
        }
        (void)recorder_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, recorder_->stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_active_ = true;
    }
    const auto stats = recorder_->stats();
    std::ostringstream detail;
    detail << "recording first packet path=" << path << " bytes=" << stats.output_bytes
           << " input_frames=" << stats.input_frames
           << " encoded_frames=" << stats.encoded_frames;
    return {MediaStatus::Ok(detail.str()), {}, path, stats};
}

MediaOperationResult MediaService::stop_recording() {
    if (!recorder_)
        return {{MediaStatusCode::NotImplemented, "recording backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!recording_accepting_ && !recording_active_ && !recorder_->active()) {
            const auto stats = recorder_->stats();
            return {MediaStatus::Ok("recording already stopped"), {}, stats.output_path, stats};
        }
        recording_accepting_ = false;
    }
    auto status = recorder_->stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_active_ = false;
        recording_failure_queued_ = false;
    }
    const auto capture_status = stop_capture_if_unused();
    if (status.ok() && !capture_status.ok()) status = capture_status;
    const auto stats = recorder_->stats();
    if (!status.ok()) return {std::move(status), {}, stats.output_path, stats};
    std::ostringstream detail;
    detail << "recording stopped path=" << stats.output_path
           << " input_frames=" << stats.input_frames
           << " encoded_frames=" << stats.encoded_frames
           << " packets=" << stats.packets << " bytes=" << stats.output_bytes;
    return {MediaStatus::Ok(detail.str()), {}, stats.output_path, stats};
}

void MediaService::handle_recording_failure(MediaStatus status) {
    RecordingFailureCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = false;
        recording_active_ = false;
        recording_failure_queued_ = false;
        callback = recording_failure_callback_;
    }
    if (recorder_) (void)recorder_->stop();
    (void)stop_capture_if_unused();
    if (callback) callback(std::move(status));
}

void MediaService::receive_frame(CapturedFrame frame) {
    auto owned = std::make_shared<CapturedFrame>(std::move(frame));
    bool publish_preview = false;
    bool submit_recording = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        latest_frame_ = owned;
        ++latest_frame_version_;
        publish_preview = preview_active_;
        submit_recording = recording_accepting_;
        if (publish_preview) ++stats_.preview_frames_published;
    }
    if (publish_preview) mailbox_->publish(owned);
    if (submit_recording && recorder_) {
        auto status = recorder_->submit(owned);
        if (!status.ok()) {
            bool notify = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                recording_accepting_ = false;
                recording_active_ = false;
                if (!recording_failure_queued_) {
                    recording_failure_queued_ = true;
                    recording_failure_status_ = status;
                    requests_.push_front({MediaOperation::RecordingFailure, {}});
                    notify = true;
                }
            }
            if (notify) work_ready_.notify_one();
        }
    }
    frame_ready_.notify_all();
}

}  // namespace cockpit::media
