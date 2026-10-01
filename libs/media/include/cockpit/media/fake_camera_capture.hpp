#pragma once

#include "cockpit/media/camera_capture.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace cockpit::media {

struct FakeCameraCaptureOptions {
    bool fail_open{false};
    bool fail_configure{false};
    bool fail_start{false};
    bool hold_start{false};
    std::chrono::milliseconds frame_interval{std::chrono::milliseconds(10)};
};

class FakeCameraCapture final : public ICameraCapture {
public:
    explicit FakeCameraCapture(FakeCameraCaptureOptions options = {});
    ~FakeCameraCapture() override;

    MediaStatus open_device(const std::string& device) override;
    MediaStatus configure(const CameraCaptureConfig& config) override;
    MediaStatus start(FrameCallback callback) override;
    MediaStatus stop() override;
    void close_device() override;
    [[nodiscard]] CameraFormat actual_format() const override;
    [[nodiscard]] CaptureStats stats() const override;
    [[nodiscard]] bool streaming() const override;

    void release_start();
    [[nodiscard]] bool wait_until_start_entered(std::chrono::milliseconds timeout);
    [[nodiscard]] std::vector<std::uint8_t> last_source_buffer() const;
    [[nodiscard]] std::uint64_t open_count() const;
    [[nodiscard]] std::uint64_t start_count() const;
    [[nodiscard]] std::uint64_t stop_count() const;

private:
    void capture_loop();

    FakeCameraCaptureOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable start_gate_;
    CameraCaptureConfig config_;
    CameraFormat actual_;
    CaptureStats stats_;
    FrameCallback callback_;
    std::thread worker_;
    std::vector<std::uint8_t> source_;
    bool opened_{false};
    bool configured_{false};
    bool streaming_{false};
    bool stop_requested_{false};
    bool start_released_{false};
    bool start_entered_{false};
    std::uint64_t next_epoch_{0};
    std::uint64_t open_count_{0};
    std::uint64_t start_count_{0};
    std::uint64_t stop_count_{0};
};

}  // namespace cockpit::media
