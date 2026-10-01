#include "cockpit/voice/vad.hpp"

#include <filesystem>

namespace cockpit::voice {

protocol::Status validate_vad_config(const VadConfig& c) {
    using protocol::StatusCode;
    if (c.sample_rate != 16000 || c.window_samples != 512 || c.threshold <= 0.0F ||
        c.threshold >= 1.0F || c.min_speech_ms < 50 || c.min_speech_ms > 2000 ||
        c.min_silence_ms < 100 || c.min_silence_ms > 5000 ||
        c.pre_roll_ms < c.min_speech_ms || c.pre_roll_ms > 2000 ||
        c.max_utterance_ms < 1000 || c.max_utterance_ms > 30000 ||
        c.max_utterance_ms <= c.min_speech_ms + c.min_silence_ms)
        return {StatusCode::INVALID_ARGUMENT, "invalid VAD engineering configuration"};
    return protocol::Status::Ok();
}

}  // namespace cockpit::voice
