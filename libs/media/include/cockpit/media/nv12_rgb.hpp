#pragma once

#include "cockpit/media/camera_capture.hpp"

#include <cstdint>
#include <vector>

namespace cockpit::media {

MediaStatus nv12_to_rgb888(const CapturedFrame& frame, std::vector<std::uint8_t>& rgb);

}  // namespace cockpit::media
