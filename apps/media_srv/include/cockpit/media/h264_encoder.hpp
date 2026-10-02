#pragma once

#include "cockpit/media/camera_capture.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cockpit::media {

struct EncoderConfig {
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

struct EncodedPacket {
    std::vector<std::uint8_t> annex_b;
    std::uint64_t camera_sequence{0};
    std::uint64_t stream_epoch{0};
    std::uint32_t rtp_timestamp{0};
    std::int64_t encoded_steady_ns{0};
    bool key_frame{false};
};

struct EncoderStats {
    std::uint64_t start_count{0};
    std::uint64_t input_frames{0};
    std::uint64_t encoded_frames{0};
    std::uint64_t packets{0};
    std::uint64_t output_bytes{0};
    std::size_t queue_peak_depth{0};
    std::uint64_t overflow_count{0};
    std::uint64_t encoder_errors{0};
    std::uint64_t idr_requests{0};
    std::int64_t first_input_steady_ns{0};
    std::int64_t last_output_steady_ns{0};
    std::string last_error;
};

using EncodedPacketCallback =
    std::function<void(std::shared_ptr<const EncodedPacket>)>;

class IH264Encoder {
public:
    virtual ~IH264Encoder() = default;
    virtual MediaStatus start(const EncoderConfig& config, const CameraFormat& format,
                              EncodedPacketCallback callback) = 0;
    virtual MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) = 0;
    virtual MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) = 0;
    virtual MediaStatus request_idr() = 0;
    virtual MediaStatus stop() = 0;
    [[nodiscard]] virtual bool active() const = 0;
    [[nodiscard]] virtual EncoderStats stats() const = 0;
};

}  // namespace cockpit::media
