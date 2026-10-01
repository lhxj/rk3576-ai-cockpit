#pragma once

#include "cockpit/media/captured_frame.hpp"

#include <functional>
#include <string>

namespace cockpit::media {

enum class MediaStatusCode {
    Ok,
    InvalidArgument,
    InvalidState,
    Unavailable,
    IoError,
    Timeout,
    UnsupportedCameraFormat,
    UnsupportedPlaneLayout,
    CameraNotStreaming,
    NotImplemented,
    Cancelled,
};

struct MediaStatus {
    MediaStatusCode code{MediaStatusCode::Ok};
    std::string detail;

    [[nodiscard]] bool ok() const noexcept { return code == MediaStatusCode::Ok; }
    static MediaStatus Ok(std::string detail = {}) {
        return {MediaStatusCode::Ok, std::move(detail)};
    }
};

struct CameraCaptureConfig {
    std::string device;
    std::string camera_id{"front"};
    std::uint32_t width{1632};
    std::uint32_t height{1224};
    std::string pixel_format{"NV12"};
    std::uint32_t fps{30};
    std::uint32_t buffer_count{4};
    int poll_timeout_ms{200};
};

using FrameCallback = std::function<void(CapturedFrame)>;

class ICameraCapture {
public:
    virtual ~ICameraCapture() = default;
    virtual MediaStatus open_device(const std::string& device) = 0;
    virtual MediaStatus configure(const CameraCaptureConfig& config) = 0;
    virtual MediaStatus start(FrameCallback callback) = 0;
    virtual MediaStatus stop() = 0;
    virtual void close_device() = 0;
    [[nodiscard]] virtual CameraFormat actual_format() const = 0;
    [[nodiscard]] virtual CaptureStats stats() const = 0;
    [[nodiscard]] virtual bool streaming() const = 0;
};

}  // namespace cockpit::media
