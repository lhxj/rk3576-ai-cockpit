#include "cockpit/vehicle/client.hpp"

#include <algorithm>
#include <utility>

namespace cockpit::vehicle {
namespace {
CommandSubmission rejected(protocol::Status status) {
    CommandSubmission submission;
    submission.status = std::move(status);
    return submission;
}
}  // namespace

CommandSubmission InProcessVehicleCoreClient::send_command(const VehicleCommand& command) {
    return core_.send_command(command);
}

VehicleState InProcessVehicleCoreClient::get_snapshot() const { return core_.get_snapshot(); }

protocol::Status InProcessVehicleCoreClient::subscribe_state(StateCallback callback) {
    return core_.subscribe_state(std::move(callback));
}

protocol::BootEpoch InProcessVehicleCoreClient::boot_epoch() const { return core_.boot_epoch(); }

LoopbackVehicleCoreClient::LoopbackVehicleCoreClient(VehicleCore& core, std::size_t queue_capacity,
                                                     std::chrono::milliseconds response_timeout)
    : core_(core), transport_(queue_capacity), response_timeout_(response_timeout) {}

LoopbackVehicleCoreClient::~LoopbackVehicleCoreClient() { stop(); }

protocol::Status LoopbackVehicleCoreClient::start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_) return {protocol::StatusCode::INVALID_STATE, "loopback already started"};
    }
    auto status = transport_.receive([this](const protocol::Message& message) { receive(message); });
    if (!status.ok()) return status;
    status = transport_.start();
    if (!status.ok()) return status;
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    return protocol::Status::Ok();
}

void LoopbackVehicleCoreClient::stop() {
    transport_.stop();
    std::unordered_map<protocol::RequestId, std::deque<std::shared_ptr<Waiter>>> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_ && waiters_.empty()) return;
        started_ = false;
        pending.swap(waiters_);
    }
    for (auto& item : pending) {
        for (const auto& waiter : item.second) {
            std::lock_guard<std::mutex> waiter_lock(waiter->mutex);
            waiter->submission = rejected({protocol::StatusCode::CANCELLED, "loopback stopped"});
            waiter->completed = true;
            waiter->ready.notify_all();
        }
    }
}

CommandSubmission LoopbackVehicleCoreClient::send_command(const VehicleCommand& command) {
    auto waiter = std::make_shared<Waiter>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) return rejected({protocol::StatusCode::INVALID_STATE, "loopback stopped"});
        waiters_[command.request_id].push_back(waiter);
    }
    protocol::Message message;
    auto status = encode_command_message(command, message);
    if (status.ok()) status = transport_.send(message);
    if (!status.ok()) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = waiters_.find(command.request_id);
        if (found != waiters_.end() && !found->second.empty()) {
            found->second.pop_back();
            if (found->second.empty()) waiters_.erase(found);
        }
        return rejected(status);
    }
    std::unique_lock<std::mutex> lock(waiter->mutex);
    if (!waiter->ready.wait_for(lock, response_timeout_, [&] { return waiter->completed; })) {
        lock.unlock();
        std::lock_guard<std::mutex> client_lock(mutex_);
        const auto found = waiters_.find(command.request_id);
        if (found != waiters_.end()) {
            auto& queue = found->second;
            queue.erase(std::remove(queue.begin(), queue.end(), waiter), queue.end());
            if (queue.empty()) waiters_.erase(found);
        }
        return rejected({protocol::StatusCode::TIMEOUT, "loopback response"});
    }
    return waiter->submission;
}

VehicleState LoopbackVehicleCoreClient::get_snapshot() const { return core_.get_snapshot(); }

protocol::Status LoopbackVehicleCoreClient::subscribe_state(StateCallback callback) {
    return core_.subscribe_state(std::move(callback));
}

protocol::BootEpoch LoopbackVehicleCoreClient::boot_epoch() const { return core_.boot_epoch(); }

void LoopbackVehicleCoreClient::receive(const protocol::Message& message) {
    const auto decoded = decode_command_message(message);
    CommandSubmission submission = decoded.status.ok()
        ? core_.send_command(decoded.command) : rejected(decoded.status);
    std::shared_ptr<Waiter> waiter;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = waiters_.find(message.header.request_id);
        if (found == waiters_.end() || found->second.empty()) return;
        waiter = found->second.front();
        found->second.pop_front();
        if (found->second.empty()) waiters_.erase(found);
    }
    {
        std::lock_guard<std::mutex> lock(waiter->mutex);
        waiter->submission = std::move(submission);
        waiter->completed = true;
    }
    waiter->ready.notify_all();
}

}  // namespace cockpit::vehicle
