#include "cockpit/audio/wav_reader.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
void put16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}
void put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) b[at + i] = static_cast<std::uint8_t>(v >> (i * 8));
}
std::vector<std::uint8_t> wav() {
    std::vector<std::uint8_t> b(48, 0);
    for (std::size_t i = 0; i < 4; ++i) {
        b[i] = static_cast<std::uint8_t>("RIFF"[i]);
        b[8 + i] = static_cast<std::uint8_t>("WAVE"[i]);
        b[12 + i] = static_cast<std::uint8_t>("fmt "[i]);
        b[36 + i] = static_cast<std::uint8_t>("data"[i]);
    }
    put32(b, 4, 40); put32(b, 16, 16); put16(b, 20, 1);
    put16(b, 22, 1); put32(b, 24, 16000); put32(b, 28, 32000);
    put16(b, 32, 2); put16(b, 34, 16); put32(b, 40, 4);
    return b;
}
void write(const std::filesystem::path& p, const std::vector<std::uint8_t>& b) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}
}  // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        const auto external = cockpit::audio::read_pcm_wav(argv[1]);
        assert(external.status.ok() && !external.pcm.bytes.empty());
    }
    const auto path = std::filesystem::temp_directory_path() /
        ("cockpit-wav-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav");
    auto b = wav();
    write(path, b);
    auto result = cockpit::audio::read_pcm_wav(path);
    assert(result.status.ok() && result.pcm.bytes.size() == 4);
    assert(result.pcm.format.sample_rate == 16000 && result.pcm.format.channels == 1);
    b[0] = 'X'; write(path, b);
    assert(!cockpit::audio::read_pcm_wav(path).status.ok());
    b = wav(); b.pop_back(); write(path, b);
    assert(!cockpit::audio::read_pcm_wav(path).status.ok());
    b = wav(); put32(b, 24, 8000); put32(b, 28, 16000); write(path, b);
    assert(cockpit::audio::read_pcm_wav(path).status.detail.find("UNSUPPORTED_SAMPLE_RATE") != std::string::npos);
    b = wav(); put16(b, 22, 2); put16(b, 32, 4); put32(b, 28, 64000); write(path, b);
    assert(!cockpit::audio::read_pcm_wav(path).status.ok());
    b = wav(); put16(b, 34, 8); write(path, b);
    assert(!cockpit::audio::read_pcm_wav(path).status.ok());
    b = wav(); put32(b, 40, 0xffffffffU); write(path, b);
    assert(!cockpit::audio::read_pcm_wav(path).status.ok());
    std::filesystem::remove(path);
}
