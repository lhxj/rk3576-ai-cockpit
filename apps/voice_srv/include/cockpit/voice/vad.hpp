#pragma once

#include "cockpit/voice/voice.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cockpit::voice {

// Engineering baseline, NOT PRODUCT-TUNED. At 16 kHz, 300 ms is 4800 frames.
struct VadConfig {
    std::uint32_t sample_rate{16000};
    float threshold{0.5F};
    std::uint32_t min_speech_ms{250};
    std::uint32_t min_silence_ms{500};
    std::uint32_t pre_roll_ms{300};
    std::uint32_t max_utterance_ms{15000};
    std::uint32_t window_samples{512};
    std::string model_path;
};

enum class VadState { Silence, SpeechStarted, SpeechActive, SpeechEnded, Error };
enum class VadEventType { SpeechStarted, SpeechEnded, Error };
struct VadEvent {
    VadEventType type{VadEventType::Error};
    std::uint64_t sample_index{0};
    std::chrono::steady_clock::time_point timestamp{};
    // -1 means the backend does not expose confidence.
    float confidence{-1.0F};
};
struct VadAcceptResult {
    protocol::Status status;
    std::vector<VadEvent> events;
};

// Called serially by the voice processing worker. No microphone ownership.
class IVadBackend {
public:
    virtual ~IVadBackend() = default;
    virtual protocol::Status configure(const VadConfig& config) = 0;
    virtual void reset() = 0;
    virtual VadAcceptResult accept_samples(const audio::PcmBuffer& pcm,
                                           std::chrono::steady_clock::time_point captured_at) = 0;
    virtual VadAcceptResult flush() = 0;
    virtual VadState state() const = 0;
};

using VadCallback = std::function<void(const VadEvent&)>;
protocol::Status validate_vad_config(const VadConfig& config);

}  // namespace cockpit::voice
