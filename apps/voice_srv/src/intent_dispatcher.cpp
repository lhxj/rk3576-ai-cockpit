#include "cockpit/voice/intent_dispatcher.hpp"

#include <chrono>
#include <utility>

namespace cockpit::voice {

VoiceIntentDispatcher::VoiceIntentDispatcher(const DeterministicIntentRouter& router,
    VoiceSessionController& controller, IVehicleCommandSink& sink, Clock clock,
    IntentDispatchCallback report, std::size_t queue_capacity, std::size_t recent_capacity)
    : router_(router), controller_(controller), sink_(sink), clock_(std::move(clock)),
      report_(std::move(report)), queue_capacity_(queue_capacity), recent_capacity_(recent_capacity) {}

VoiceIntentDispatcher::~VoiceIntentDispatcher() { stop(); }

std::size_t VoiceIntentDispatcher::FinalKeyHash::operator()(const FinalKey& key) const {
    std::size_t hash = 0;
    for (const auto value : {key.epoch, key.session, key.generation, key.request, key.sequence})
        hash ^= std::hash<std::uint64_t>{}(value) + static_cast<std::size_t>(0x9e3779b9U) +
                (hash << 6) + (hash >> 2);
    return hash;
}

protocol::Status VoiceIntentDispatcher::start() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    if (running_ || stopping_ || worker_.joinable())
        return {protocol::StatusCode::INVALID_STATE, "intent dispatcher already started"};
    if (!clock_ || queue_capacity_ == 0 || recent_capacity_ == 0)
        return {protocol::StatusCode::INVALID_ARGUMENT, "intent dispatcher configuration"};
    queue_ = std::make_shared<ipc::BoundedQueue<IntentInput>>(queue_capacity_);
    recent_order_.clear();
    recent_set_.clear();
    running_ = true;
    try {
        worker_ = std::thread(&VoiceIntentDispatcher::run, this, queue_);
    } catch (...) {
        running_ = false;
        queue_.reset();
        return {protocol::StatusCode::INTERNAL_ERROR, "intent dispatcher worker start"};
    }
    return protocol::Status::Ok();
}

protocol::Status VoiceIntentDispatcher::enqueue_event(const AsrEvent& event,
                                                       protocol::Deadline deadline_ms) {
    if (event.type != AsrEventType::FINAL) return protocol::Status::Ok();
    // Keep queue memory bounded as well as queue depth. The router's text limit is 512 bytes.
    if (event.text.size() > 512 || event.status.detail.size() > 512)
        return {protocol::StatusCode::INVALID_ARGUMENT, "intent FINAL size"};
    std::shared_ptr<ipc::BoundedQueue<IntentInput>> queue;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (!running_ || !queue_) return {protocol::StatusCode::INVALID_STATE, "intent dispatcher stopped"};
        queue = queue_;
    }
    const auto pushed = queue->try_push({event, deadline_ms});
    if (pushed == ipc::QueueStatus::FULL) {
        ++overflow_;
        return {protocol::StatusCode::UNAVAILABLE, "INTENT_DISPATCH_QUEUE_OVERFLOW"};
    }
    if (pushed != ipc::QueueStatus::OK)
        return {protocol::StatusCode::INVALID_STATE, "intent dispatcher queue closed"};
    ++enqueued_;
    return protocol::Status::Ok();
}

bool VoiceIntentDispatcher::remember(const FinalKey& key) {
    if (!recent_set_.insert(key).second) return false;
    recent_order_.push_back(key);
    if (recent_order_.size() > recent_capacity_) {
        recent_set_.erase(recent_order_.front());
        recent_order_.pop_front();
    }
    const auto peak = cache_peak_.load();
    if (recent_order_.size() > peak) cache_peak_ = recent_order_.size();
    return true;
}

void VoiceIntentDispatcher::run(std::shared_ptr<ipc::BoundedQueue<IntentInput>> queue) {
    while (running_) {
        IntentInput input;
        const auto popped = queue->pop_for(input, std::chrono::milliseconds(50));
        if (!running_ || popped == ipc::QueueStatus::CLOSED) break;
        if (popped != ipc::QueueStatus::OK) continue;
        ++processed_;
        const FinalKey key{input.event.token.boot_epoch, input.event.token.session_id,
                           input.event.token.generation, input.event.token.request_id,
                           input.event.sequence};
        IntentDispatchReport result;
        result.token = input.event.token;
        result.asr_sequence = input.event.sequence;
        if (!remember(key)) {
            ++duplicates_;
            result.duplicate = true;
        } else {
            const auto matched = router_.match(input, controller_, clock_());
            result.outcome = matched.outcome;
            if (matched.outcome == IntentOutcome::MATCH) {
                result.status = router_.dispatch(matched, controller_, sink_, clock_());
                result.submitted = result.status.ok();
                if (result.submitted) ++submitted_;
            }
        }
        if (report_) {
            try { report_(result); } catch (...) {}
        }
    }
}

void VoiceIntentDispatcher::stop() {
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (stopping_ || (!running_ && !worker_.joinable())) return;
        stopping_ = true;
        running_ = false;
        if (queue_) queue_->close();
        worker = std::move(worker_);
    }
    if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        queue_.reset();
        stopping_ = false;
    }
}

IntentDispatchStats VoiceIntentDispatcher::stats() const {
    return {enqueued_.load(), processed_.load(), duplicates_.load(), submitted_.load(),
            overflow_.load(), cache_peak_.load(), queue_capacity_};
}

}  // namespace cockpit::voice
