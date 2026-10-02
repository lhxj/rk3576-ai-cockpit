#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cockpit::media {

using H264Nal = std::vector<std::uint8_t>;

std::vector<H264Nal> split_annex_b(const std::vector<std::uint8_t>& bytes);
bool contains_h264_nal_type(const std::vector<std::uint8_t>& bytes,
                            std::uint8_t type);
std::string base64_encode(const std::vector<std::uint8_t>& bytes);
std::string h264_profile_level_id(const std::vector<std::uint8_t>& sps);

}  // namespace cockpit::media
