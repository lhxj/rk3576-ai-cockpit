#pragma once

#include "cockpit/voice/intent_dispatcher.hpp"
#include "cockpit/voice/vad_live_pipeline.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace cockpit::voice {

enum class VoiceRuntimeState { STOPPED, STARTING, LISTENING, PROCESSING, STOPPING, ERROR };

struct VoiceRuntimeConfig {
    VadConfig vad;
    audio::AudioFormat requested_audio;
    std::chrono::milliseconds intent_timeout{5000};
    std::size_t audio_queue_capacity{0};
    std::size_t dispatch_queue_capacity{32};
    std::size_t recent_final_capacity{128};
};

struct VoiceRuntimeMetrics {
    std::uint64_t utterance_count{0};
    std::uint64_t final_count{0};
    std::uint64_t intent_match_count{0};
    std::uint64_t no_match_count{0};
    std::uint64_t rejected_count{0};
    std::uint64_t duplicate_final_count{0};
    std::size_t dispatch_queue_peak{0};
    std::size_t audio_queue_peak{0};
    std::uint64_t xrun_count{0};
    std::uint64_t audio_overflow_count{0};
};

// Owns the live voice workers and intent dispatcher. Capture/VAD/ASR models,
// controller, command sink, VehicleCore, and MediaService are owned by the
// enclosing application and must outlive this object.
class VoiceRuntime final {
public:
    using Clock = VoiceIntentDispatcher::Clock;
    using ReportCallback = IntentDispatchCallback;
    using XrunCounter = std::function<std::uint64_t()>;

    VoiceRuntime(audio::IAudioCapture& capture, IVadBackend& vad, IAsrBackend& asr,
                 VoiceSessionController& controller,
                 const DeterministicIntentRouter& router,
                 IVoiceSessionCommandSink& command_sink, Clock clock,
                 VoiceRuntimeConfig config = {}, ReportCallback report = {},
                 XrunCounter xrun_counter = {});
    ~VoiceRuntime();
    VoiceRuntime(const VoiceRuntime&) = delete;
    VoiceRuntime& operator=(const VoiceRuntime&) = delete;

    protocol::Status start();
    protocol::Status stop();
    protocol::Status cancel_current();
    VoiceRuntimeState state() const;
    VoiceRuntimeMetrics metrics() const;

    // Deterministic integration seam only. It enters the same bounded FINAL
    // handoff as the real ASR callback and is never evidence of live voice control.
    protocol::Status inject_final_for_test(const std::string& text, bool duplicate = false);
    protocol::Status inject_partial_for_test(const std::string& text);

private:
    void on_asr_event(const AsrEvent& event);
    void on_vad_event(const VadEvent& event);
    void on_dispatch_report(const IntentDispatchReport& report);
    protocol::Status inject_for_test(AsrEventType type, const std::string& text,
                                     bool duplicate);
    void set_state(VoiceRuntimeState state);

    audio::IAudioCapture& capture_;
    VoiceSessionController& controller_;
    IVoiceSessionCommandSink& command_sink_;
    Clock clock_;
    const VoiceRuntimeConfig config_;
    ReportCallback report_;
    XrunCounter xrun_counter_;
    VoiceIntentDispatcher dispatcher_;
    VadLivePipeline pipeline_;
    mutable std::mutex mutex_;
    VoiceRuntimeState state_{VoiceRuntimeState::STOPPED};
    VoiceRuntimeMetrics counters_;
    std::atomic<bool> accepting_events_{false};
    std::atomic<protocol::RequestId> next_test_request_{1000000};
    std::atomic<std::uint64_t> next_test_sequence_{1000000};
};

const char* voice_runtime_state_name(VoiceRuntimeState state);

}  // namespace cockpit::voice
