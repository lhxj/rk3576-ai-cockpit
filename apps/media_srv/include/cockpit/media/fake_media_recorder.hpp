#pragma once

#include "cockpit/media/media_recorder.hpp"

#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <thread>

namespace cockpit::media {

struct FakeMediaRecorderOptions {
    bool fail_start{false};
    bool fail_encode{false};
    std::size_t fail_after_packets{0};
    std::chrono::milliseconds encode_delay{std::chrono::milliseconds(0)};
};

class FakeMediaRecorder final : public IMediaRecorder {
public:
    explicit FakeMediaRecorder(FakeMediaRecorderOptions options = {});
    ~FakeMediaRecorder() override;

    MediaStatus start(const RecorderConfig& config, const CameraFormat& format,
                      const std::string& output_path) override;
    MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) override;
    MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] RecorderStats stats() const override;

private:
    void run();
    void fail_locked(MediaStatus status);

    const FakeMediaRecorderOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable first_packet_;
    RecorderConfig config_;
    CameraFormat format_;
    std::deque<std::shared_ptr<const CapturedFrame>> queue_;
    std::ofstream output_;
    std::thread worker_;
    RecorderStats stats_;
    MediaStatus terminal_status_;
    bool active_{false};
    bool accepting_{false};
    bool stop_requested_{false};
    bool first_packet_written_{false};
};

}  // namespace cockpit::media
