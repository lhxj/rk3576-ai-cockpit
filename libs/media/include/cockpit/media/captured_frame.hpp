#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cockpit::media {

enum class TimestampClock { Unknown, Monotonic, Realtime };

struct CameraFormat {
    std::string camera_id{"front"};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string pixel_format;
    std::uint32_t num_planes{0};
    std::uint32_t bytes_per_line{0};
    std::uint32_t size_image{0};
    std::uint32_t requested_buffers{0};
    std::uint32_t actual_buffers{0};
    std::uint32_t fps_numerator{0};
    std::uint32_t fps_denominator{0};
    bool frame_interval_supported{false};
    std::string driver;
};

struct CapturedFrame {
    std::string camera_id;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string pixel_format;
    std::uint32_t bytes_per_line{0};
    std::uint32_t size_image{0};
    std::uint32_t bytes_used{0};
    std::uint64_t sequence{0};
    std::uint64_t stream_epoch{0};
    std::int64_t capture_timestamp_ns{0};
    std::int64_t dequeue_steady_timestamp_ns{0};
    TimestampClock timestamp_clock{TimestampClock::Unknown};
    std::vector<std::uint8_t> payload;
};

struct CaptureStats {
    std::uint64_t frames{0};
    std::uint64_t sequence_gap_count{0};
    std::uint64_t poll_timeouts{0};
    std::uint64_t dequeue_errors{0};
    std::uint64_t queue_errors{0};
    std::uint32_t bytes_used_min{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t bytes_used_max{0};
    std::uint64_t stream_epoch{0};
    std::int64_t first_dequeue_steady_ns{0};
    std::int64_t last_dequeue_steady_ns{0};
};

}  // namespace cockpit::media
