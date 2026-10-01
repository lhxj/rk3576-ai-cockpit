#include "cockpit/voice/sherpa_vad.hpp"

#include <sherpa-onnx/c-api/c-api.h>

#include <filesystem>
#include <vector>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;
}

struct SherpaVadBackend::Handle {
    const SherpaOnnxVoiceActivityDetector* vad{nullptr};
};

SherpaVadBackend::~SherpaVadBackend() {
    if (handle_) {
        SherpaOnnxDestroyVoiceActivityDetector(handle_->vad);
        delete handle_;
    }
}

Status SherpaVadBackend::configure(const VadConfig& config) {
    auto status = validate_vad_config(config);
    if (!status.ok()) return status;
    if (handle_) return {StatusCode::INVALID_STATE, "VAD already loaded"};
    if (config.model_path.empty() || !std::filesystem::is_regular_file(config.model_path))
        return {StatusCode::UNAVAILABLE, "VAD_MODEL_NOT_FOUND"};
    // v1.11.3 can call exit(-1) for an unsupported Silero ONNX shape.
    // Constrain this adapter to the audited v5.0 asset; deployment also checks SHA-256.
    std::error_code ec;
    if (std::filesystem::file_size(config.model_path, ec) != 2313101 || ec)
        return {StatusCode::INVALID_ARGUMENT, "VAD_MODEL_UNSUPPORTED: expected pinned Silero v5.0"};
    SherpaOnnxVadModelConfig c{};
    c.silero_vad.model = config.model_path.c_str();
    c.silero_vad.threshold = config.threshold;
    c.silero_vad.min_silence_duration = config.min_silence_ms / 1000.0F;
    c.silero_vad.min_speech_duration = config.min_speech_ms / 1000.0F;
    c.silero_vad.max_speech_duration = config.max_utterance_ms / 1000.0F;
    c.silero_vad.window_size = static_cast<int32_t>(config.window_samples);
    c.sample_rate = static_cast<int32_t>(config.sample_rate);
    c.num_threads = 1;
    c.provider = "cpu";
    // The C API's circular buffer needs only enough for one bounded utterance.
    const auto* p = SherpaOnnxCreateVoiceActivityDetector(
        &c, config.max_utterance_ms / 1000.0F + config.min_silence_ms / 1000.0F + 1.0F);
    if (!p) return {StatusCode::UNAVAILABLE, "VAD_MODEL_LOAD_FAILED"};
    handle_ = new Handle{p};
    config_ = config;
    ++load_count_;
    reset();
    return Status::Ok();
}

void SherpaVadBackend::reset() {
    if (handle_) SherpaOnnxVoiceActivityDetectorReset(handle_->vad);
    state_ = VadState::Silence;
    samples_seen_ = 0;
}

VadAcceptResult SherpaVadBackend::collect(std::chrono::steady_clock::time_point at) {
    VadAcceptResult result{Status::Ok(), {}};
    const bool detected = SherpaOnnxVoiceActivityDetectorDetected(handle_->vad) != 0;
    if (detected && state_ == VadState::Silence) {
        state_ = VadState::SpeechStarted;
        result.events.push_back({VadEventType::SpeechStarted, samples_seen_, at, -1.0F});
    } else if (detected) {
        state_ = VadState::SpeechActive;
    }
    while (!SherpaOnnxVoiceActivityDetectorEmpty(handle_->vad)) {
        const auto* segment = SherpaOnnxVoiceActivityDetectorFront(handle_->vad);
        const std::uint64_t end = segment && segment->start >= 0 && segment->n >= 0 ?
            static_cast<std::uint64_t>(segment->start) + static_cast<std::uint64_t>(segment->n) : samples_seen_;
        SherpaOnnxDestroySpeechSegment(segment);
        SherpaOnnxVoiceActivityDetectorPop(handle_->vad);
        state_ = VadState::SpeechEnded;
        result.events.push_back({VadEventType::SpeechEnded, end, at, -1.0F});
    }
    if (state_ == VadState::SpeechEnded) state_ = VadState::Silence;
    return result;
}

VadAcceptResult SherpaVadBackend::accept_samples(const audio::PcmBuffer& pcm,
                                                  std::chrono::steady_clock::time_point at) {
    if (!handle_) return {{StatusCode::INVALID_STATE, "VAD not loaded"}, {}};
    if (pcm.format.sample_rate != config_.sample_rate || pcm.format.channels != 1 ||
        pcm.format.sample_format != audio::SampleFormat::S16_LE || pcm.bytes.empty() ||
        pcm.bytes.size() > 6400 || (pcm.bytes.size() & 1U))
        return {{StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: VAD PCM"}, {}};
    std::vector<float> samples(pcm.bytes.size() / 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto raw = static_cast<std::uint16_t>(pcm.bytes[2 * i] |
            (static_cast<std::uint16_t>(pcm.bytes[2 * i + 1]) << 8));
        const auto signed_sample = raw < 32768U ? static_cast<std::int32_t>(raw) :
            static_cast<std::int32_t>(raw) - 65536;
        samples[i] = static_cast<float>(signed_sample) / 32768.0F;
    }
    SherpaOnnxVoiceActivityDetectorAcceptWaveform(handle_->vad, samples.data(),
                                                   static_cast<int32_t>(samples.size()));
    samples_seen_ += samples.size();
    return collect(at);
}

VadAcceptResult SherpaVadBackend::flush() {
    if (!handle_) return {{StatusCode::INVALID_STATE, "VAD not loaded"}, {}};
    SherpaOnnxVoiceActivityDetectorFlush(handle_->vad);
    return collect(std::chrono::steady_clock::now());
}

}  // namespace cockpit::voice
