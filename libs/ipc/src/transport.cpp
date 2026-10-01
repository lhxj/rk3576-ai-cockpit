#include "cockpit/ipc/transport.hpp"

#include <chrono>
#include <utility>

namespace cockpit::ipc {

InMemoryTransport::InMemoryTransport(std::size_t capacity) : queue_(capacity) {}
InMemoryTransport::~InMemoryTransport() { stop(); }

protocol::Status InMemoryTransport::receive(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != TransportState::STOPPED || ever_started_ || !callback)
        return {protocol::StatusCode::INVALID_STATE, "set callback before first start"};
    callback_ = std::move(callback);
    return protocol::Status::Ok();
}

protocol::Status InMemoryTransport::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ever_started_ || !callback_)
        return {protocol::StatusCode::INVALID_STATE, "callback missing or transport already used"};
    ever_started_ = true;
    state_ = TransportState::RUNNING;
    worker_ = std::thread(&InMemoryTransport::run, this);
    return protocol::Status::Ok();
}

void InMemoryTransport::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = TransportState::STOPPED;
        queue_.close();
    }
    if (worker_.joinable()) worker_.join();
}

protocol::Status InMemoryTransport::send(const protocol::Message& message) {
    std::vector<std::uint8_t> wire;
    auto encoded = protocol::encode(message, wire);
    if (!encoded.ok()) return encoded;
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != TransportState::RUNNING)
        return {protocol::StatusCode::INVALID_STATE, "transport stopped"};
    const auto pushed = queue_.try_push(std::move(wire));
    if (pushed == QueueStatus::FULL) return {protocol::StatusCode::UNAVAILABLE, "queue full"};
    if (pushed == QueueStatus::CLOSED) return {protocol::StatusCode::INVALID_STATE, "closed"};
    return protocol::Status::Ok();
}

TransportState InMemoryTransport::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void InMemoryTransport::run() {
    std::vector<std::uint8_t> wire;
    while (queue_.pop_for(wire, std::chrono::milliseconds(250)) != QueueStatus::CLOSED) {
        if (wire.empty()) continue;
        if (state() != TransportState::RUNNING) break;
        auto decoded = protocol::decode(wire);
        if (decoded.status.ok()) {
            try { callback_(decoded.message); }
            catch (...) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    state_ = TransportState::ERROR;
                    queue_.close();
                }
                break;
            }
        }
        wire.clear();
    }
}

}  // namespace cockpit::ipc
