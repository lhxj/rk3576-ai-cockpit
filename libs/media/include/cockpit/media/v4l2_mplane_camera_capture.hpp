#pragma once

#include "cockpit/media/camera_capture.hpp"

#ifdef COCKPIT_ENABLE_V4L2_CAMERA

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace cockpit::media {

class V4l2MplaneCameraCapture final : public ICameraCapture {
public:
    V4l2MplaneCameraCapture() = default;
    ~V4l2MplaneCameraCapture() override;

    MediaStatus open_device(const std::string& device) override;
    MediaStatus configure(const CameraCaptureConfig& config) override;
    MediaStatus start(FrameCallback callback) override;
    MediaStatus stop() override;
    void close_device() override;
    [[nodiscard]] CameraFormat actual_format() const override;
    [[nodiscard]] CaptureStats stats() const override;
    [[nodiscard]] bool streaming() const override;

private:
    struct Mapping { void* address{nullptr}; std::size_t length{0}; };
    MediaStatus request_and_map_buffers();
    void release_buffers();
    void capture_loop();
    MediaStatus errno_status(const char* operation) const;

    mutable std::mutex mutex_;
    int fd_{-1};
    CameraCaptureConfig config_;
    CameraFormat actual_;
    CaptureStats stats_;
    std::vector<Mapping> mappings_;
    FrameCallback callback_;
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    bool configured_{false};
    bool streaming_{false};
    bool stream_on_{false};
    std::uint64_t next_epoch_{0};
};

}  // namespace cockpit::media

#endif
