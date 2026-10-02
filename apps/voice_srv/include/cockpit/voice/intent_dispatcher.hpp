#pragma once

#include "cockpit/ipc/bounded_queue.hpp"
#include "cockpit/voice/intent_router.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace cockpit::voice {

struct IntentDispatchReport {
    SessionToken token;
    std::uint64_t asr_sequence{0};
    IntentOutcome outcome{IntentOutcome::INVALID_INPUT};
    protocol::Status status;
    bool duplicate{false};
    bool submitted{false};
};
using IntentDispatchCallback = std::function<void(const IntentDispatchReport&)>;
using IntentDispatchPrepare =
    std::function<protocol::Status(SessionToken, protocol::Deadline)>;

struct IntentDispatchStats {
    std::uint64_t enqueued{0};
    std::uint64_t processed{0};
    std::uint64_t duplicates{0};
    std::uint64_t submitted{0};
    std::uint64_t overflow{0};
    std::size_t cache_peak{0};
    std::size_t queue_peak{0};
    std::size_t queue_capacity{0};
};

// enqueue_event is safe in VoiceSessionController::deliver_event's callback:
// it only copies FINAL into a bounded queue and never calls the controller/Core.
// The report callback runs on the owned worker and must be quick/non-reentrant.
class VoiceIntentDispatcher final {
public:
    using Clock = std::function<protocol::Deadline()>;
    VoiceIntentDispatcher(const DeterministicIntentRouter& router,
                          VoiceSessionController& controller, IVehicleCommandSink& sink,
                          Clock clock, IntentDispatchCallback report,
                          std::size_t queue_capacity = 32, std::size_t recent_capacity = 128,
                          IntentDispatchPrepare prepare = {});
    ~VoiceIntentDispatcher();
    VoiceIntentDispatcher(const VoiceIntentDispatcher&) = delete;
    VoiceIntentDispatcher& operator=(const VoiceIntentDispatcher&) = delete;

    protocol::Status start();
    protocol::Status enqueue_event(const AsrEvent& event, protocol::Deadline deadline_ms);
    void stop();
    IntentDispatchStats stats() const;

private:
    struct FinalKey {
        protocol::BootEpoch epoch;
        protocol::SessionId session;
        std::uint64_t generation;
        protocol::RequestId request;
        std::uint64_t sequence;
        bool operator==(const FinalKey& other) const {
            return epoch == other.epoch && session == other.session &&
                   generation == other.generation && request == other.request &&
                   sequence == other.sequence;
        }
    };
    struct FinalKeyHash {
        std::size_t operator()(const FinalKey& key) const;
    };
    void run(std::shared_ptr<ipc::BoundedQueue<IntentInput>> queue);
    bool remember(const FinalKey& key);

    const DeterministicIntentRouter& router_;
    VoiceSessionController& controller_;
    IVehicleCommandSink& sink_;
    Clock clock_;
    IntentDispatchCallback report_;
    IntentDispatchPrepare prepare_;
    const std::size_t queue_capacity_;
    const std::size_t recent_capacity_;
    mutable std::mutex lifecycle_mutex_;
    std::shared_ptr<ipc::BoundedQueue<IntentInput>> queue_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> accepting_{false};
    bool stopping_{false}; // guarded by lifecycle_mutex_
    std::deque<FinalKey> recent_order_; // worker-owned
    std::unordered_set<FinalKey, FinalKeyHash> recent_set_; // worker-owned
    std::atomic<std::uint64_t> enqueued_{0};
    std::atomic<std::uint64_t> processed_{0};
    std::atomic<std::uint64_t> duplicates_{0};
    std::atomic<std::uint64_t> submitted_{0};
    std::atomic<std::uint64_t> overflow_{0};
    std::atomic<std::size_t> cache_peak_{0};
    std::atomic<std::size_t> queue_peak_{0};
};

}  // namespace cockpit::voice
