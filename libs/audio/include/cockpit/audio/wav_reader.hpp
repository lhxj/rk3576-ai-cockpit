#pragma once

#include "cockpit/audio/audio.hpp"

#include <filesystem>

namespace cockpit::audio {

struct WavReadResult {
    protocol::Status status;
    PcmBuffer pcm;
    double duration_seconds{0.0};
};

// File input only. No device access and no implicit resampling.
WavReadResult read_pcm_wav(const std::filesystem::path& path);

}  // namespace cockpit::audio
