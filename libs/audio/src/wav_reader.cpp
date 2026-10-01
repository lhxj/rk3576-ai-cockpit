#include "cockpit/audio/wav_reader.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

namespace cockpit::audio {
namespace {
constexpr std::uintmax_t kMaxWavBytes = 128U * 1024U * 1024U;

std::uint16_t le16(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::uint16_t>(b[p] | (static_cast<std::uint16_t>(b[p + 1]) << 8));
}
std::uint32_t le32(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::uint32_t>(b[p]) | (static_cast<std::uint32_t>(b[p + 1]) << 8) |
           (static_cast<std::uint32_t>(b[p + 2]) << 16) | (static_cast<std::uint32_t>(b[p + 3]) << 24);
}
bool tag(const std::vector<std::uint8_t>& b, std::size_t p, const char* s) {
    return b[p] == static_cast<std::uint8_t>(s[0]) && b[p + 1] == static_cast<std::uint8_t>(s[1]) &&
           b[p + 2] == static_cast<std::uint8_t>(s[2]) && b[p + 3] == static_cast<std::uint8_t>(s[3]);
}
WavReadResult fail(protocol::StatusCode code, const char* why) {
    return {{code, why}, {}, 0.0};
}
}  // namespace

WavReadResult read_pcm_wav(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return fail(protocol::StatusCode::UNAVAILABLE, "WAV file not found or inaccessible");
    if (size < 44 || size > kMaxWavBytes) return fail(protocol::StatusCode::INVALID_ARGUMENT, "WAV size outside limit");
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail(protocol::StatusCode::UNAVAILABLE, "WAV open failed");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (static_cast<std::size_t>(in.gcount()) != bytes.size())
        return fail(protocol::StatusCode::MALFORMED, "WAV truncated during read");
    if (!tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE"))
        return fail(protocol::StatusCode::MALFORMED, "RIFF/WAVE signature");
    const std::uint64_t riff_end = static_cast<std::uint64_t>(le32(bytes, 4)) + 8U;
    if (riff_end != bytes.size()) return fail(protocol::StatusCode::MALFORMED, "RIFF length mismatch");
    bool have_fmt = false;
    bool have_data = false;
    std::uint32_t sample_rate = 0;
    std::size_t data_at = 0;
    std::uint32_t data_size = 0;
    for (std::size_t at = 12; at < bytes.size();) {
        if (bytes.size() - at < 8) return fail(protocol::StatusCode::MALFORMED, "truncated chunk header");
        const std::uint32_t length = le32(bytes, at + 4);
        const std::uint64_t end = static_cast<std::uint64_t>(at) + 8U + length;
        if (end > bytes.size()) return fail(protocol::StatusCode::MALFORMED, "truncated chunk payload");
        if (tag(bytes, at, "fmt ")) {
            if (have_fmt || length < 16) return fail(protocol::StatusCode::MALFORMED, "invalid fmt chunk");
            have_fmt = true;
            const auto encoding = le16(bytes, at + 8);
            const auto channels = le16(bytes, at + 10);
            sample_rate = le32(bytes, at + 12);
            const auto byte_rate = le32(bytes, at + 16);
            const auto align = le16(bytes, at + 20);
            const auto bits = le16(bytes, at + 22);
            if (encoding != 1) return fail(protocol::StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_ENCODING: require PCM");
            if (sample_rate != 16000)
                return fail(protocol::StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_SAMPLE_RATE: require 16000 Hz");
            if (channels != 1) return fail(protocol::StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_CHANNELS: require mono");
            if (bits != 16) return fail(protocol::StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_BIT_DEPTH: require S16");
            if (align != 2 || byte_rate != 32000)
                return fail(protocol::StatusCode::MALFORMED, "invalid PCM byte rate or alignment");
        } else if (tag(bytes, at, "data")) {
            if (have_data) return fail(protocol::StatusCode::MALFORMED, "duplicate data chunk");
            have_data = true;
            data_at = at + 8;
            data_size = length;
        }
        const std::uint64_t next = end + (length & 1U);
        if (next > bytes.size()) return fail(protocol::StatusCode::MALFORMED, "missing chunk padding");
        at = static_cast<std::size_t>(next);
    }
    if (!have_fmt || !have_data || data_size == 0 || (data_size & 1U))
        return fail(protocol::StatusCode::MALFORMED, "missing or invalid PCM data");
    WavReadResult result;
    result.pcm.format = {sample_rate, 1, SampleFormat::S16_LE, 320};
    result.pcm.bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(data_at),
                            bytes.begin() + static_cast<std::ptrdiff_t>(data_at + data_size));
    result.duration_seconds = static_cast<double>(data_size / 2U) / sample_rate;
    return result;
}

}  // namespace cockpit::audio
