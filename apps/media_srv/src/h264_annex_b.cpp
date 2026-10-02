#include "cockpit/media/h264_annex_b.hpp"

#include <array>
#include <iomanip>
#include <sstream>

namespace cockpit::media {
namespace {

std::size_t start_code(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    if (at + 3 <= bytes.size() && bytes[at] == 0 && bytes[at + 1] == 0 &&
        bytes[at + 2] == 1) return 3;
    if (at + 4 <= bytes.size() && bytes[at] == 0 && bytes[at + 1] == 0 &&
        bytes[at + 2] == 0 && bytes[at + 3] == 1) return 4;
    return 0;
}

}  // namespace

std::vector<H264Nal> split_annex_b(const std::vector<std::uint8_t>& bytes) {
    std::vector<H264Nal> result;
    std::size_t cursor = 0;
    while (cursor < bytes.size()) {
        std::size_t code = 0;
        while (cursor < bytes.size() && (code = start_code(bytes, cursor)) == 0) ++cursor;
        if (cursor == bytes.size()) break;
        const auto begin = cursor + code;
        cursor = begin;
        while (cursor < bytes.size() && start_code(bytes, cursor) == 0) ++cursor;
        auto end = cursor;
        while (end > begin && bytes[end - 1] == 0) --end;
        if (end > begin) result.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                                              bytes.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return result;
}

bool contains_h264_nal_type(const std::vector<std::uint8_t>& bytes, std::uint8_t type) {
    for (const auto& nal : split_annex_b(bytes))
        if (!nal.empty() && (nal.front() & 0x1FU) == type) return true;
    return false;
}

std::string base64_encode(const std::vector<std::uint8_t>& bytes) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((bytes.size() + 2U) / 3U) * 4U);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 3U) {
        const std::uint32_t a = bytes[offset];
        const std::uint32_t b = offset + 1U < bytes.size() ? bytes[offset + 1U] : 0U;
        const std::uint32_t c = offset + 2U < bytes.size() ? bytes[offset + 2U] : 0U;
        const std::uint32_t value = (a << 16U) | (b << 8U) | c;
        output.push_back(table[(value >> 18U) & 0x3FU]);
        output.push_back(table[(value >> 12U) & 0x3FU]);
        output.push_back(offset + 1U < bytes.size() ? table[(value >> 6U) & 0x3FU] : '=');
        output.push_back(offset + 2U < bytes.size() ? table[value & 0x3FU] : '=');
    }
    return output;
}

std::string h264_profile_level_id(const std::vector<std::uint8_t>& sps) {
    if (sps.size() < 4U) return "000000";
    std::ostringstream output;
    output << std::hex << std::uppercase << std::setfill('0')
           << std::setw(2) << static_cast<unsigned>(sps[1])
           << std::setw(2) << static_cast<unsigned>(sps[2])
           << std::setw(2) << static_cast<unsigned>(sps[3]);
    return output.str();
}

}  // namespace cockpit::media
