#pragma once

#include "cockpit/ipc/bounded_queue.hpp"
#include "cockpit/voice/vad_utterance.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace cockpit::voice {

struct VadLiveMetrics {
    std::uint64_t captured_frames{0};
    std::uint64_t queue_overflow_count{0};
    std::size_t queue_capacity{0};
    std::size_t queue_peak_depth{0};
    VadUtteranceMetrics utterance;
};

// Owns two joinable workers; audio_srv's IAudioCapture alone owns the PCM device.
class VadLivePipeline final {
public:
    VadLivePipeline(audio::IAudioCapture& capture, IVadBackend& vad, IAsrBackend& asr,
                    VoiceSessionController& controller, AsrCallback asr_callback,
                    VadCallback vad_callback = {}, std::size_t capacity_override = 0);
    ~VadLivePipeline();
    protocol::Status start(const VadConfig& config, const audio::AudioFormat& requested = {});
    protocol::Status cancel_current();
    protocol::Status stop();
    bool running() const { return running_; }
    bool failed() const { return failed_; }
    UtteranceState utterance_state() const { return processor_.state(); }
    VadLiveMetrics metrics() const;

private:
    void capture_loop();
    void processing_loop();
    void fail(protocol::Status status);

    audio::IAudioCapture& capture_;
    VadUtteranceProcessor processor_;
    const std::size_t capacity_override_;
    std::unique_ptr<ipc::BoundedQueue<audio::PcmChunk>> queue_;
    std::thread capture_thread_;
    std::thread processing_thread_;
    audio::AudioFormat actual_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<bool> failed_{false};
    mutable std::mutex mutex_;
    protocol::Status failure_;
    VadLiveMetrics metrics_;
};

}  // namespace cockpit::voice
