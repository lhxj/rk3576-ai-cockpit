#pragma once

#include "cockpit/voice/vad.hpp"

#include <atomic>
#include <deque>
#include <mutex>

namespace cockpit::voice {

enum class UtteranceState { Listening, Starting, Speaking, Finalizing, Cancelled, Error };

struct VadUtteranceMetrics {
    std::uint64_t vad_speech_start_count{0};
    std::uint64_t vad_speech_end_count{0};
    std::uint64_t utterance_count{0};
    std::uint64_t rejected_short_utterance_count{0};
    std::uint64_t forced_max_duration_count{0};
    std::uint64_t pcm_sequence_gap_count{0};
    std::uint64_t pre_roll_frames{0};
    std::uint64_t speech_frames{0};
    std::uint64_t silence_frames{0};
    std::uint64_t partial_count{0};
    std::size_t peak_pre_roll_frames{0};
    double vad_processing_ms{0};
    double speech_start_to_first_partial_ms{-1};
    double speech_end_to_final_ms{-1};
};

// A serial PCM consumer. The caller owns VAD/ASR model lifetimes. cancel_current()
// may be called from another thread and fences its token before returning.
class VadUtteranceProcessor final {
public:
    VadUtteranceProcessor(IVadBackend& vad, IAsrBackend& asr, VoiceSessionController& controller,
                          AsrCallback asr_callback, VadCallback vad_callback = {});
    protocol::Status configure(const VadConfig& config);
    protocol::Status accept(const audio::PcmChunk& chunk);
    protocol::Status flush();
    protocol::Status cancel_current();
    void stop();
    UtteranceState state() const { return state_.load(); }
    VadUtteranceMetrics metrics() const;
    std::size_t pre_roll_size_frames() const;

private:
    protocol::Status start_utterance(std::chrono::steady_clock::time_point at);
    protocol::Status feed(const audio::PcmBuffer& pcm);
    protocol::Status finish_utterance(bool forced, std::chrono::steady_clock::time_point at);
    protocol::Status process_events(const std::vector<VadEvent>& events,
                                    std::chrono::steady_clock::time_point at);
    void backend_event(const AsrEvent& event);
    void append_pre_roll(const audio::PcmBuffer& pcm);
    void cancel_on_worker();
    void fail(protocol::Status status);

    IVadBackend& vad_;
    IAsrBackend& asr_;
    VoiceSessionController& controller_;
    AsrCallback asr_callback_;
    VadCallback vad_callback_;
    VadConfig config_;
    bool configured_{false};
    std::atomic<UtteranceState> state_{UtteranceState::Listening};
    std::atomic<bool> cancel_requested_{false};
    mutable std::mutex token_mutex_;
    SessionToken token_;
    std::uint64_t next_request_id_{1};
    std::uint64_t expected_sequence_{0};
    std::uint64_t active_frames_{0};
    std::deque<std::uint8_t> pre_roll_;
    mutable std::mutex metrics_mutex_;
    VadUtteranceMetrics metrics_;
    bool final_seen_{false};
    std::chrono::steady_clock::time_point speech_started_at_{};
    std::chrono::steady_clock::time_point speech_ended_at_{};
};

}  // namespace cockpit::voice
