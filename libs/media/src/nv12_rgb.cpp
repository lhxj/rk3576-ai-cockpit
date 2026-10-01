#include "cockpit/media/nv12_rgb.hpp"

#include <algorithm>
#include <cstddef>

namespace cockpit::media {
namespace {

std::uint8_t clamp_channel(int value) {
    return static_cast<std::uint8_t>(std::max(0, std::min(255, value)));
}

}  // namespace

MediaStatus nv12_to_rgb888(const CapturedFrame& frame, std::vector<std::uint8_t>& rgb) {
    if (frame.pixel_format != "NV12" || frame.width == 0 || frame.height == 0 ||
        frame.bytes_per_line < frame.width || (frame.width % 2) != 0 ||
        (frame.height % 2) != 0)
        return {MediaStatusCode::UnsupportedCameraFormat, "NV12 geometry/stride"};
    const auto y_size = static_cast<std::size_t>(frame.bytes_per_line) * frame.height;
    const auto uv_size = static_cast<std::size_t>(frame.bytes_per_line) * (frame.height / 2);
    if (frame.payload.size() < y_size + uv_size || frame.bytes_used < y_size + uv_size)
        return {MediaStatusCode::InvalidArgument, "NV12 payload shorter than stride layout"};

    rgb.resize(static_cast<std::size_t>(frame.width) * frame.height * 3);
    const auto* y_plane = frame.payload.data();
    const auto* uv_plane = frame.payload.data() + y_size;
    for (std::uint32_t y = 0; y < frame.height; ++y) {
        for (std::uint32_t x = 0; x < frame.width; ++x) {
            const int luma = y_plane[static_cast<std::size_t>(y) * frame.bytes_per_line + x];
            const auto uv_index = static_cast<std::size_t>(y / 2) * frame.bytes_per_line +
                                  static_cast<std::size_t>(x & ~1U);
            const int u = static_cast<int>(uv_plane[uv_index]) - 128;
            const int v = static_cast<int>(uv_plane[uv_index + 1]) - 128;
            const int red = luma + ((359 * v) >> 8);
            const int green = luma - ((88 * u + 183 * v) >> 8);
            const int blue = luma + ((454 * u) >> 8);
            const auto out = (static_cast<std::size_t>(y) * frame.width + x) * 3;
            rgb[out] = clamp_channel(red);
            rgb[out + 1] = clamp_channel(green);
            rgb[out + 2] = clamp_channel(blue);
        }
    }
    return MediaStatus::Ok();
}

}  // namespace cockpit::media
