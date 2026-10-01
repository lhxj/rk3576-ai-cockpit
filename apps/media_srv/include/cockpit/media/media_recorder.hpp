#pragma once

#include "cockpit/media/camera_capture.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace cockpit::media {

struct RecorderConfig {
    std::uint32_t fps_numerator{30};
    std::uint32_t fps_denominator{1};
    std::uint32_t bitrate_target{8'000'000};
    std::uint32_t bitrate_min{7'500'000};
    std::uint32_t bitrate_max{8'500'000};
    std::uint32_t gop{60};
    std::uint32_t h264_profile{100};
    std::uint32_t h264_level{40};
    std::size_t queue_capacity{12};
    std::chrono::milliseconds first_packet_timeout{std::chrono::milliseconds(3000)};
};

struct RecorderStats {
    std::uint64_t input_frames{0};
    std::uint64_t encoded_frames{0};
    std::uint64_t packets{0};
    std::uint64_t output_bytes{0};
    std::size_t queue_peak_depth{0};
    std::uint64_t overflow_count{0};
    std::uint64_t encoder_errors{0};
    std::int64_t first_input_steady_ns{0};
    std::int64_t last_output_steady_ns{0};
    bool file_closed{true};
    std::string output_path;
    std::string last_error;
};

class IMediaRecorder {
public:
    virtual ~IMediaRecorder() = default;
    virtual MediaStatus start(const RecorderConfig& config, const CameraFormat& format,
                              const std::string& output_path) = 0;
    virtual MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) = 0;
    virtual MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) = 0;
    virtual MediaStatus stop() = 0;
    [[nodiscard]] virtual bool active() const = 0;
    [[nodiscard]] virtual RecorderStats stats() const = 0;
};

}  // namespace cockpit::media
