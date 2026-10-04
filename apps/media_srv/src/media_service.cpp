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
                           std::unique_ptr<IH264Encoder> encoder,
                           std::unique_ptr<IFileRecordingSink> file_sink,
                           std::unique_ptr<IRtspServer> rtsp_server)
    : config_(std::move(config)), capture_(std::move(capture)),
      encoder_(std::move(encoder)), file_sink_(std::move(file_sink)),
      rtsp_server_(std::move(rtsp_server)), mailbox_(std::move(mailbox)) {}

MediaService::~MediaService() { stop(); }

MediaStatus MediaService::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return MediaStatus::Ok("already running");
    if (!capture_ || !mailbox_ || config_.capture.device.empty() ||
        config_.snapshot_directory.empty() || config_.operation_capacity == 0 ||
        ((encoder_ || file_sink_) && (!encoder_ || !file_sink_ ||
          config_.recording_directory.empty() || config_.encoder.queue_capacity == 0 ||
          config_.recording_packet_queue_capacity == 0)) ||
        (rtsp_server_ && !encoder_))
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
        rtsp_accepting_ = false;
        vision_active_ = false;
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
    recording_sink_active_ = false;
    rtsp_accepting_ = false;
    rtsp_active_ = false;
    rtsp_sink_active_ = false;
    vision_active_ = false;
    recording_failure_queued_ = false;
    rtsp_failure_queued_ = false;
    encoding_failure_queued_ = false;
    latest_frame_.reset();
}

MediaStatus MediaService::submit(MediaOperation operation, MediaOperationCompletion completion) {
    if (!completion || operation == MediaOperation::RecordingSinkFailure ||
        operation == MediaOperation::RtspSinkFailure ||
        operation == MediaOperation::EncodingFailure)
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

bool MediaService::rtsp_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rtsp_active_;
}

bool MediaService::vision_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return vision_active_;
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
    return file_sink_ ? file_sink_->stats() : RecorderStats{};
}

EncoderStats MediaService::encoder_stats() const {
    return encoder_ ? encoder_->stats() : EncoderStats{};
}
RtspStats MediaService::rtsp_stats() const {
    return rtsp_server_ ? rtsp_server_->stats() : RtspStats{};
}

void MediaService::set_recording_failure_callback(RecordingFailureCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    recording_failure_callback_ = std::move(callback);
}

void MediaService::set_rtsp_failure_callback(RecordingFailureCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    rtsp_failure_callback_ = std::move(callback);
}

void MediaService::set_vision_frame_callback(VisionFrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    vision_frame_callback_ = std::move(callback);
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
        if (!result.status.ok() && request.operation != MediaOperation::RecordingSinkFailure &&
            request.operation != MediaOperation::RtspSinkFailure &&
            request.operation != MediaOperation::EncodingFailure) {
            std::lock_guard<std::mutex> lock(mutex_);
            ++stats_.operation_failures;
        }
        if (request.completion) request.completion(std::move(result));
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = false;
        rtsp_accepting_ = false;
        vision_active_ = false;
    }
    if (encoder_) (void)encoder_->stop();
    if (file_sink_) (void)file_sink_->stop();
    if (rtsp_server_) (void)rtsp_server_->stop();
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
        case MediaOperation::RtspStart: ++stats_.rtsp_start_requests; break;
        case MediaOperation::RtspStop: ++stats_.rtsp_stop_requests; break;
        case MediaOperation::VisionStart: ++stats_.vision_start_requests; break;
        case MediaOperation::VisionStop: ++stats_.vision_stop_requests; break;
        case MediaOperation::RecordingSinkFailure:
        case MediaOperation::RtspSinkFailure:
        case MediaOperation::EncodingFailure: break;
        }
    }
    switch (operation) {
    case MediaOperation::PreviewStart: return start_preview();
    case MediaOperation::PreviewStop: return stop_preview();
    case MediaOperation::Snapshot: return snapshot();
    case MediaOperation::RecordingStart: return start_recording();
    case MediaOperation::RecordingStop: return stop_recording();
    case MediaOperation::RtspStart: return start_rtsp();
    case MediaOperation::RtspStop: return stop_rtsp();
    case MediaOperation::VisionStart: return start_vision();
    case MediaOperation::VisionStop: return stop_vision();
    case MediaOperation::RecordingSinkFailure: {
        MediaStatus status;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status = recording_failure_status_;
        }
        handle_recording_sink_failure(status);
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    case MediaOperation::RtspSinkFailure: {
        MediaStatus status;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status = rtsp_failure_status_;
        }
        handle_rtsp_sink_failure(status);
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    case MediaOperation::EncodingFailure: {
        MediaStatus status;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status = encoding_failure_status_;
        }
        handle_encoding_failure(status);
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
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
        if (preview_active_ || recording_accepting_ || recording_active_ ||
            rtsp_accepting_ || rtsp_active_ || vision_active_)
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
    if (!recording_supported())
        return {{MediaStatusCode::NotImplemented, "recording backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (recording_active_ || recording_accepting_) {
            const auto stats = file_sink_->stats();
            return {MediaStatus::Ok("recording already active"), {}, stats.output_path,
                    stats, encoder_stats(), rtsp_stats()};
        }
        recording_failure_queued_ = false;
        recording_failure_status_ = MediaStatus::Ok();
        encoding_failure_queued_ = false;
        encoding_failure_status_ = MediaStatus::Ok();
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
    status = file_sink_->start(path, config_.recording_packet_queue_capacity);
    if (!status.ok()) {
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, file_sink_->stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_sink_active_ = true;
    }
    status = ensure_encoder();
    if (!status.ok()) {
        { std::lock_guard<std::mutex> lock(mutex_); recording_sink_active_ = false; }
        (void)file_sink_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, file_sink_->stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = true;
    }
    (void)encoder_->request_idr();
    status = start_capture();
    if (!status.ok()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recording_accepting_ = false;
        }
        (void)stop_encoder_if_unused();
        { std::lock_guard<std::mutex> lock(mutex_); recording_sink_active_ = false; }
        (void)file_sink_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, file_sink_->stats(), encoder_stats(), rtsp_stats()};
    }
    status = encoder_->wait_for_first_packet(config_.encoder.first_packet_timeout);
    if (status.ok())
        status = file_sink_->wait_for_first_packet(config_.encoder.first_packet_timeout);
    if (!status.ok()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recording_accepting_ = false;
        }
        (void)stop_encoder_if_unused();
        { std::lock_guard<std::mutex> lock(mutex_); recording_sink_active_ = false; }
        (void)file_sink_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, path, file_sink_->stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_active_ = true;
    }
    const auto stats = file_sink_->stats();
    const auto encoder = encoder_->stats();
    std::ostringstream detail;
    detail << "recording first packet path=" << path << " bytes=" << stats.output_bytes
           << " input_frames=" << stats.input_frames
           << " encoded_frames=" << encoder.encoded_frames;
    return {MediaStatus::Ok(detail.str()), {}, path, stats, encoder, rtsp_stats()};
}

MediaOperationResult MediaService::stop_recording() {
    if (!recording_supported())
        return {{MediaStatusCode::NotImplemented, "recording backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!recording_accepting_ && !recording_active_ && !file_sink_->active()) {
            const auto stats = file_sink_->stats();
            return {MediaStatus::Ok("recording already stopped"), {}, stats.output_path,
                    stats, encoder_stats(), rtsp_stats()};
        }
        recording_accepting_ = false;
        recording_active_ = false;
    }
    auto status = stop_encoder_if_unused();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_sink_active_ = false;
    }
    const auto file_status = file_sink_->stop();
    if (status.ok() && !file_status.ok()) status = file_status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_failure_queued_ = false;
    }
    const auto capture_status = stop_capture_if_unused();
    if (status.ok() && !capture_status.ok()) status = capture_status;
    const auto stats = file_sink_->stats();
    if (!status.ok()) return {std::move(status), {}, stats.output_path, stats,
                              encoder_stats(), rtsp_stats()};
    std::ostringstream detail;
    detail << "recording stopped path=" << stats.output_path
           << " input_frames=" << stats.input_frames
           << " encoded_frames=" << stats.encoded_frames
           << " packets=" << stats.packets << " bytes=" << stats.output_bytes;
    return {MediaStatus::Ok(detail.str()), {}, stats.output_path, stats,
            encoder_stats(), rtsp_stats()};
}

MediaStatus MediaService::ensure_encoder() {
    if (!encoder_) return {MediaStatusCode::NotImplemented, "H.264 encoder unavailable"};
    if (encoder_->active()) return MediaStatus::Ok("shared encoder already active");
    return encoder_->start(config_.encoder, capture_->actual_format(),
        [this](std::shared_ptr<const EncodedPacket> packet) {
            receive_encoded(std::move(packet));
        });
}

MediaStatus MediaService::stop_encoder_if_unused() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (recording_accepting_ || rtsp_accepting_ || recording_active_ || rtsp_active_)
            return MediaStatus::Ok("shared encoder retained by consumer");
    }
    return encoder_ ? encoder_->stop() : MediaStatus::Ok();
}

MediaOperationResult MediaService::start_rtsp() {
    if (!rtsp_supported())
        return {{MediaStatusCode::NotImplemented, "RTSP backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (rtsp_active_ || rtsp_accepting_)
            return {MediaStatus::Ok("RTSP already active"), {}, {}, recorder_stats(),
                    encoder_stats(), rtsp_stats()};
        rtsp_failure_queued_ = false;
        rtsp_failure_status_ = MediaStatus::Ok();
        encoding_failure_queued_ = false;
        encoding_failure_status_ = MediaStatus::Ok();
    }
    auto status = prepare_capture();
    if (!status.ok()) return {std::move(status), {}, {}, recorder_stats(),
                              encoder_stats(), rtsp_stats()};
    auto stream_format = capture_->actual_format();
    // SDP describes the encoded stream. Some Rockchip V4L2 mainpath drivers do
    // not implement G_PARM, so the encoder's configured rate is authoritative.
    stream_format.fps_numerator = config_.encoder.fps_numerator;
    stream_format.fps_denominator = config_.encoder.fps_denominator;
    status = rtsp_server_->start(config_.rtsp, stream_format, [this] {
        if (encoder_) (void)encoder_->request_idr();
    });
    if (!status.ok()) {
        (void)stop_capture_if_unused();
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rtsp_sink_active_ = true;
    }
    status = ensure_encoder();
    if (!status.ok()) {
        { std::lock_guard<std::mutex> lock(mutex_); rtsp_sink_active_ = false; }
        (void)rtsp_server_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rtsp_accepting_ = true;
    }
    (void)encoder_->request_idr();
    status = start_capture();
    if (!status.ok()) {
        { std::lock_guard<std::mutex> lock(mutex_); rtsp_accepting_ = false; }
        (void)stop_encoder_if_unused();
        { std::lock_guard<std::mutex> lock(mutex_); rtsp_sink_active_ = false; }
        (void)rtsp_server_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    status = encoder_->wait_for_first_packet(config_.encoder.first_packet_timeout);
    if (status.ok())
        status = rtsp_server_->wait_for_parameters(config_.encoder.first_packet_timeout);
    if (!status.ok()) {
        { std::lock_guard<std::mutex> lock(mutex_); rtsp_accepting_ = false; }
        (void)stop_encoder_if_unused();
        { std::lock_guard<std::mutex> lock(mutex_); rtsp_sink_active_ = false; }
        (void)rtsp_server_->stop();
        (void)stop_capture_if_unused();
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rtsp_active_ = true;
    }
    const auto network = rtsp_stats();
    std::ostringstream detail;
    detail << "RTSP ready port=" << network.listen_port << " path=" << config_.rtsp.path;
    return {MediaStatus::Ok(detail.str()), {}, {}, recorder_stats(), encoder_stats(), network};
}

MediaOperationResult MediaService::stop_rtsp() {
    if (!rtsp_supported())
        return {{MediaStatusCode::NotImplemented, "RTSP backend unavailable"}, {}, {}, {}};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!rtsp_accepting_ && !rtsp_active_ && !rtsp_server_->active())
            return {MediaStatus::Ok("RTSP already stopped"), {}, {}, recorder_stats(),
                    encoder_stats(), rtsp_stats()};
        rtsp_accepting_ = false;
        rtsp_active_ = false;
        rtsp_sink_active_ = false;
    }
    auto status = rtsp_server_->stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rtsp_failure_queued_ = false;
    }
    const auto encoder_status = stop_encoder_if_unused();
    if (status.ok() && !encoder_status.ok()) status = encoder_status;
    const auto capture_status = stop_capture_if_unused();
    if (status.ok() && !capture_status.ok()) status = capture_status;
    return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
}

MediaOperationResult MediaService::start_vision() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (vision_active_)
            return {MediaStatus::Ok("vision already active"), {}, {}, recorder_stats(),
                    encoder_stats(), rtsp_stats()};
        if (!vision_frame_callback_)
            return {{MediaStatusCode::InvalidState, "vision callback unavailable"},
                    {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    }
    auto status = start_capture();
    if (!status.ok())
        return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        vision_active_ = true;
    }
    return {MediaStatus::Ok("vision consuming shared CAM0 frames"), {}, {},
            recorder_stats(), encoder_stats(), rtsp_stats()};
}

MediaOperationResult MediaService::stop_vision() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!vision_active_)
            return {MediaStatus::Ok("vision already stopped"), {}, {}, recorder_stats(),
                    encoder_stats(), rtsp_stats()};
        vision_active_ = false;
    }
    auto status = stop_capture_if_unused();
    return {std::move(status), {}, {}, recorder_stats(), encoder_stats(), rtsp_stats()};
}

void MediaService::handle_recording_sink_failure(MediaStatus status) {
    RecordingFailureCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_accepting_ = false;
        recording_active_ = false;
        recording_sink_active_ = false;
        recording_failure_queued_ = false;
        callback = recording_failure_callback_;
    }
    if (file_sink_) (void)file_sink_->stop();
    (void)stop_encoder_if_unused();
    (void)stop_capture_if_unused();
    if (callback) callback(std::move(status));
}

void MediaService::handle_rtsp_sink_failure(MediaStatus status) {
    RecordingFailureCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rtsp_accepting_ = false;
        rtsp_active_ = false;
        rtsp_sink_active_ = false;
        rtsp_failure_queued_ = false;
        callback = rtsp_failure_callback_;
    }
    if (rtsp_server_) (void)rtsp_server_->stop();
    (void)stop_encoder_if_unused();
    (void)stop_capture_if_unused();
    if (callback) callback(std::move(status));
}

void MediaService::handle_encoding_failure(MediaStatus status) {
    RecordingFailureCallback recording_callback;
    RecordingFailureCallback rtsp_callback;
    bool recording_failed = false;
    bool rtsp_failed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_failed = recording_accepting_ || recording_active_ || recording_sink_active_;
        rtsp_failed = rtsp_accepting_ || rtsp_active_ || rtsp_sink_active_;
        recording_accepting_ = false;
        rtsp_accepting_ = false;
        recording_active_ = false;
        rtsp_active_ = false;
        encoding_failure_queued_ = false;
        recording_callback = recording_failure_callback_;
        rtsp_callback = rtsp_failure_callback_;
    }
    if (encoder_) (void)encoder_->stop();
    if (file_sink_) (void)file_sink_->stop();
    if (rtsp_server_) (void)rtsp_server_->stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_sink_active_ = false;
        rtsp_sink_active_ = false;
    }
    (void)stop_capture_if_unused();
    if (recording_failed && recording_callback) recording_callback(status);
    if (rtsp_failed && rtsp_callback) rtsp_callback(std::move(status));
}

void MediaService::receive_encoded(std::shared_ptr<const EncodedPacket> packet) {
    bool recording = false;
    bool rtsp = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        recording = recording_sink_active_;
        rtsp = rtsp_sink_active_;
    }
    MediaStatus recording_status = MediaStatus::Ok();
    MediaStatus rtsp_status = MediaStatus::Ok();
    if (recording && file_sink_) {
        recording_status = file_sink_->submit(packet);
    }
    if (rtsp && rtsp_server_) {
        rtsp_status = rtsp_server_->submit(packet);
    }
    if (!recording_status.ok() || !rtsp_status.ok()) {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!recording_status.ok() && recording_sink_active_ &&
                !recording_failure_queued_) {
                recording_failure_queued_ = true;
                recording_failure_status_ = recording_status;
                requests_.push_front({MediaOperation::RecordingSinkFailure, {}});
                notify = true;
            }
            if (!rtsp_status.ok() && rtsp_sink_active_ && !rtsp_failure_queued_) {
                rtsp_failure_queued_ = true;
                rtsp_failure_status_ = rtsp_status;
                requests_.push_front({MediaOperation::RtspSinkFailure, {}});
                notify = true;
            }
        }
        if (notify) work_ready_.notify_one();
    }
}

void MediaService::receive_frame(CapturedFrame frame) {
    auto owned = std::make_shared<CapturedFrame>(std::move(frame));
    bool publish_preview = false;
    bool submit_encoding = false;
    VisionFrameCallback submit_vision;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        latest_frame_ = owned;
        ++latest_frame_version_;
        publish_preview = preview_active_;
        submit_encoding = recording_accepting_ || rtsp_accepting_;
        if (vision_active_) {
            submit_vision = vision_frame_callback_;
            if (submit_vision) ++stats_.vision_frames_submitted;
        }
        if (publish_preview) ++stats_.preview_frames_published;
    }
    if (publish_preview) mailbox_->publish(owned);
    if (submit_vision) submit_vision(owned);
    if (submit_encoding && encoder_) {
        auto status = encoder_->submit(owned);
        if (!status.ok()) {
            bool notify = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                recording_accepting_ = false;
                rtsp_accepting_ = false;
                if (!encoding_failure_queued_) {
                    encoding_failure_queued_ = true;
                    encoding_failure_status_ = status;
                    requests_.push_front({MediaOperation::EncodingFailure, {}});
                    notify = true;
                }
            }
            if (notify) work_ready_.notify_one();
        }
    }
    frame_ready_.notify_all();
}

}  // namespace cockpit::media
