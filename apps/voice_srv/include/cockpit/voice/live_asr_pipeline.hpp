#pragma once

#include "cockpit/ipc/bounded_queue.hpp"
#include "cockpit/voice/voice.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace cockpit::voice {

struct LiveAsrMetrics {
    std::uint64_t captured_frames{0};
    std::uint64_t partial_count{0};
    std::uint64_t queue_overflow_count{0};
    std::size_t queue_capacity{0};
    std::size_t max_queue_depth{0};
    double first_partial_ms{-1};
    double final_after_capture_stop_ms{-1};
    double capture_stop_ms{-1};
    double session_stop_ms{-1};
};

// Caller owns capture, backend, controller. start/finish/cancel are called by
// the session owner; the two workers are always joined before reuse/destruction.
class LiveAsrPipeline final {
public:
    LiveAsrPipeline(audio::IAudioCapture& capture, IAsrBackend& backend,
                    VoiceSessionController& controller, AsrCallback callback,
                    std::size_t queue_capacity_override = 0);
    ~LiveAsrPipeline();
    protocol::Status start(protocol::RequestId request_id, const audio::AudioFormat& requested = {});
    protocol::Status finish();
    protocol::Status cancel();
    bool running() const { return running_; }
    bool failed() const { return failed_; }
    SessionToken token() const { return token_; }
    LiveAsrMetrics metrics() const;

private:
    void capture_loop();
    void decode_loop();
    void backend_event(const AsrEvent& event);
    void fail(protocol::Status status);
    void stop_workers(bool cancelled);

    audio::IAudioCapture& capture_;
    IAsrBackend& backend_;
    VoiceSessionController& controller_;
    AsrCallback callback_;
    const std::size_t capacity_override_;
    std::unique_ptr<ipc::BoundedQueue<audio::PcmChunk>> queue_;
    std::thread capture_thread_;
    std::thread decode_thread_;
    SessionToken token_;
    audio::AudioFormat actual_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> final_seen_{false};
    mutable std::mutex data_mutex_;
    protocol::Status failure_;
    LiveAsrMetrics metrics_;
    std::chrono::steady_clock::time_point started_at_;
    std::chrono::steady_clock::time_point capture_stopped_at_;
};

}  // namespace cockpit::voice
