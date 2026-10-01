#include "cockpit/infer/infer.hpp"

#include <utility>

namespace cockpit::infer {

protocol::Status SerialInferenceScheduler::try_acquire(ModelKind kind) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (occupied_) return {protocol::StatusCode::UNAVAILABLE, "inference slot busy"};
    occupied_ = true;
    owner_ = kind;
    return protocol::Status::Ok();
}

void SerialInferenceScheduler::release(ModelKind kind) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (occupied_ && owner_ == kind) occupied_ = false;
}

MockLanguageModelBackend::MockLanguageModelBackend(IInferenceScheduler& scheduler,
    std::vector<std::string> chunks, std::chrono::milliseconds delay)
    : scheduler_(scheduler), chunks_(std::move(chunks)), delay_(delay) {}

MockLanguageModelBackend::~MockLanguageModelBackend() { unload(); }

protocol::Status MockLanguageModelBackend::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != ModelState::Unloaded) return {protocol::StatusCode::INVALID_STATE, "already loaded"};
    state_ = ModelState::Loading;
    state_ = ModelState::Ready;
    return protocol::Status::Ok();
}

void MockLanguageModelBackend::unload() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        wake_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ModelState::Unloaded;
    active_session_ = 0;
}

protocol::Status MockLanguageModelBackend::generate(LanguageRequest request, LanguageCallback callback) {
    if (request.request_id == 0 || request.session_id == 0 || request.prompt.empty() || !callback)
        return {protocol::StatusCode::INVALID_ARGUMENT, "language request"};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Ready) return {protocol::StatusCode::INVALID_STATE, "model not ready"};
    }
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != ModelState::Ready) return {protocol::StatusCode::INVALID_STATE, "model not ready"};
    auto slot = scheduler_.try_acquire(ModelKind::Language);
    if (!slot.ok()) return slot;
    state_ = ModelState::Busy;
    active_session_ = request.session_id;
    cancelled_ = false;
    try {
        worker_ = std::thread(&MockLanguageModelBackend::run, this, std::move(request), std::move(callback));
    } catch (...) {
        state_ = ModelState::Error;
        active_session_ = 0;
        scheduler_.release(ModelKind::Language);
        return {protocol::StatusCode::INTERNAL_ERROR, "worker start"};
    }
    return protocol::Status::Ok();
}

protocol::Status MockLanguageModelBackend::cancel(protocol::SessionId session_id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Busy || active_session_ != session_id)
            return {protocol::StatusCode::INVALID_STATE, "session not active"};
        cancelled_ = true;
        wake_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
    return protocol::Status::Ok();
}

ModelState MockLanguageModelBackend::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void MockLanguageModelBackend::run(LanguageRequest request, LanguageCallback callback) {
    std::unique_lock<std::mutex> lock(mutex_);
    for (const auto& chunk : chunks_) {
        if (wake_.wait_for(lock, delay_, [this] { return cancelled_; })) break;
        try {
            callback({request.request_id, request.session_id, request.generation,
                      chunk, false, protocol::Status::Ok()});
        } catch (...) { state_ = ModelState::Error; cancelled_ = true; break; }
    }
    if (!cancelled_) {
        try {
            callback({request.request_id, request.session_id, request.generation,
                      {}, true, protocol::Status::Ok()});
        } catch (...) { state_ = ModelState::Error; }
    }
    if (state_ != ModelState::Error) state_ = ModelState::Ready;
    active_session_ = 0;
    scheduler_.release(ModelKind::Language);
}

}  // namespace cockpit::infer
