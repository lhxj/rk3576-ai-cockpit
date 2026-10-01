#pragma once

#include "cockpit/media/camera_capture.hpp"
#include "cockpit/media/preview_mailbox.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cockpit::media {

enum class MediaOperation { PreviewStart, PreviewStop, Snapshot };

struct MediaOperationResult {
    MediaStatus status;
    std::shared_ptr<const CapturedFrame> frame;
    std::string output_path;
};

using MediaOperationCompletion = std::function<void(MediaOperationResult)>;

struct MediaServiceConfig {
    CameraCaptureConfig capture;
    std::string snapshot_directory;
    std::size_t operation_capacity{16};
    std::chrono::milliseconds snapshot_wait{std::chrono::milliseconds(1000)};
};

struct MediaServiceStats {
    std::uint64_t preview_frames_published{0};
    std::uint64_t snapshots_written{0};
    std::uint64_t operation_failures{0};
};

class MediaService {
public:
    MediaService(MediaServiceConfig config, std::unique_ptr<ICameraCapture> capture,
                 std::shared_ptr<PreviewMailbox> mailbox = std::make_shared<PreviewMailbox>());
    ~MediaService();

    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;

    MediaStatus start();
    void stop();
    MediaStatus submit(MediaOperation operation, MediaOperationCompletion completion);

    [[nodiscard]] bool streaming() const;
    [[nodiscard]] CameraFormat actual_format() const;
    [[nodiscard]] CaptureStats capture_stats() const;
    [[nodiscard]] MediaServiceStats service_stats() const;
    [[nodiscard]] std::shared_ptr<PreviewMailbox> preview_mailbox() const { return mailbox_; }
    [[nodiscard]] std::shared_ptr<const CapturedFrame> latest_frame() const;

private:
    struct Request { MediaOperation operation; MediaOperationCompletion completion; };
    void run();
    MediaOperationResult execute(MediaOperation operation);
    MediaOperationResult start_preview();
    MediaOperationResult stop_preview();
    MediaOperationResult snapshot();
    void receive_frame(CapturedFrame frame);

    const MediaServiceConfig config_;
    std::unique_ptr<ICameraCapture> capture_;
    std::shared_ptr<PreviewMailbox> mailbox_;
    mutable std::mutex mutex_;
    std::condition_variable work_ready_;
    std::condition_variable frame_ready_;
    std::deque<Request> requests_;
    std::thread worker_;
    std::shared_ptr<const CapturedFrame> latest_frame_;
    std::uint64_t latest_frame_version_{0};
    MediaServiceStats stats_;
    bool running_{false};
    bool stopping_{false};
    bool streaming_{false};
};

}  // namespace cockpit::media
