#pragma once

#include "cockpit/media/camera_capture.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/h264_encoder.hpp"
#include "cockpit/media/preview_mailbox.hpp"
#include "cockpit/media/rtsp_server.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace cockpit::media {

enum class MediaOperation {
    PreviewStart,
    PreviewStop,
    Snapshot,
    RecordingStart,
    RecordingStop,
    RtspStart,
    RtspStop,
    RecordingSinkFailure,
    RtspSinkFailure,
    EncodingFailure,
};

struct MediaOperationResult {
    MediaStatus status;
    std::shared_ptr<const CapturedFrame> frame;
    std::string output_path;
    RecorderStats recorder;
    EncoderStats encoder;
    RtspStats rtsp;

    MediaOperationResult(MediaStatus status_value = {},
                         std::shared_ptr<const CapturedFrame> frame_value = {},
                         std::string path_value = {}, RecorderStats recorder_value = {},
                         EncoderStats encoder_value = {}, RtspStats rtsp_value = {})
        : status(std::move(status_value)), frame(std::move(frame_value)),
          output_path(std::move(path_value)), recorder(std::move(recorder_value)),
          encoder(std::move(encoder_value)), rtsp(std::move(rtsp_value)) {}
};

using MediaOperationCompletion = std::function<void(MediaOperationResult)>;

struct MediaServiceConfig {
    CameraCaptureConfig capture;
    std::string snapshot_directory;
    std::string recording_directory;
    EncoderConfig encoder;
    RtspConfig rtsp;
    std::size_t recording_packet_queue_capacity{64};
    std::size_t operation_capacity{16};
    std::chrono::milliseconds snapshot_wait{std::chrono::milliseconds(1000)};
};

struct MediaServiceStats {
    std::uint64_t preview_frames_published{0};
    std::uint64_t preview_start_requests{0};
    std::uint64_t preview_stop_requests{0};
    std::uint64_t snapshot_requests{0};
    std::uint64_t snapshots_written{0};
    std::uint64_t recording_start_requests{0};
    std::uint64_t recording_stop_requests{0};
    std::uint64_t rtsp_start_requests{0};
    std::uint64_t rtsp_stop_requests{0};
    std::uint64_t operation_failures{0};
};

using RecordingFailureCallback = std::function<void(MediaStatus)>;

class MediaService {
public:
    MediaService(MediaServiceConfig config, std::unique_ptr<ICameraCapture> capture,
                 std::shared_ptr<PreviewMailbox> mailbox = std::make_shared<PreviewMailbox>(),
                 std::unique_ptr<IH264Encoder> encoder = {},
                 std::unique_ptr<IFileRecordingSink> file_sink = {},
                 std::unique_ptr<IRtspServer> rtsp_server = {});
    ~MediaService();

    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;

    MediaStatus start();
    void stop();
    MediaStatus submit(MediaOperation operation, MediaOperationCompletion completion);

    [[nodiscard]] bool streaming() const;
    [[nodiscard]] bool preview_active() const;
    [[nodiscard]] bool recording_active() const;
    [[nodiscard]] bool rtsp_active() const;
    [[nodiscard]] bool recording_supported() const {
        return encoder_ != nullptr && file_sink_ != nullptr;
    }
    [[nodiscard]] bool rtsp_supported() const {
        return encoder_ != nullptr && rtsp_server_ != nullptr;
    }
    [[nodiscard]] CameraFormat actual_format() const;
    [[nodiscard]] CaptureStats capture_stats() const;
    [[nodiscard]] MediaServiceStats service_stats() const;
    [[nodiscard]] std::shared_ptr<PreviewMailbox> preview_mailbox() const { return mailbox_; }
    [[nodiscard]] std::shared_ptr<const CapturedFrame> latest_frame() const;
    [[nodiscard]] RecorderStats recorder_stats() const;
    [[nodiscard]] EncoderStats encoder_stats() const;
    [[nodiscard]] RtspStats rtsp_stats() const;
    void set_recording_failure_callback(RecordingFailureCallback callback);
    void set_rtsp_failure_callback(RecordingFailureCallback callback);

private:
    struct Request { MediaOperation operation; MediaOperationCompletion completion; };
    void run();
    MediaOperationResult execute(MediaOperation operation);
    MediaOperationResult start_preview();
    MediaOperationResult stop_preview();
    MediaOperationResult snapshot();
    MediaOperationResult start_recording();
    MediaOperationResult stop_recording();
    MediaOperationResult start_rtsp();
    MediaOperationResult stop_rtsp();
    MediaStatus prepare_capture();
    MediaStatus start_capture();
    MediaStatus stop_capture_if_unused();
    MediaStatus stop_capture();
    MediaStatus ensure_encoder();
    MediaStatus stop_encoder_if_unused();
    void handle_recording_sink_failure(MediaStatus status);
    void handle_rtsp_sink_failure(MediaStatus status);
    void handle_encoding_failure(MediaStatus status);
    void receive_encoded(std::shared_ptr<const EncodedPacket> packet);
    void receive_frame(CapturedFrame frame);

    const MediaServiceConfig config_;
    std::unique_ptr<ICameraCapture> capture_;
    std::unique_ptr<IH264Encoder> encoder_;
    std::unique_ptr<IFileRecordingSink> file_sink_;
    std::unique_ptr<IRtspServer> rtsp_server_;
    std::shared_ptr<PreviewMailbox> mailbox_;
    mutable std::mutex mutex_;
    std::condition_variable work_ready_;
    std::condition_variable frame_ready_;
    std::deque<Request> requests_;
    std::thread worker_;
    std::shared_ptr<const CapturedFrame> latest_frame_;
    std::uint64_t latest_frame_version_{0};
    MediaServiceStats stats_;
    RecordingFailureCallback recording_failure_callback_;
    RecordingFailureCallback rtsp_failure_callback_;
    bool running_{false};
    bool stopping_{false};
    bool capture_prepared_{false};
    bool capture_active_{false};
    bool preview_active_{false};
    bool recording_accepting_{false};
    bool recording_active_{false};
    bool recording_sink_active_{false};
    bool rtsp_accepting_{false};
    bool rtsp_active_{false};
    bool rtsp_sink_active_{false};
    bool recording_failure_queued_{false};
    bool rtsp_failure_queued_{false};
    bool encoding_failure_queued_{false};
    MediaStatus recording_failure_status_;
    MediaStatus rtsp_failure_status_;
    MediaStatus encoding_failure_status_;
    std::uint64_t next_recording_id_{0};
};

}  // namespace cockpit::media
