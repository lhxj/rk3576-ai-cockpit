#include "cockpit/vehicle/service_adapter.hpp"

#include <utility>

namespace cockpit::vehicle {

MockServiceAdapter::MockServiceAdapter(ServiceDomain domain, bool simulated_results)
    : domain_(domain), simulated_results_(simulated_results) {}

DispatchReceipt MockServiceAdapter::dispatch(const VehicleCommand& command,
                                             AdapterCompletion completion) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++invocations_[command.command_type];
    const auto found = behaviors_.find(command.command_type);
    const auto behavior = found == behaviors_.end() ? MockBehavior::SUCCESS : found->second;
    if (behavior == MockBehavior::SUCCESS)
        return {protocol::Status::Ok(), AdapterResult{protocol::Status::Ok(), simulated_results_}};
    if (behavior == MockBehavior::FAILURE)
        return {protocol::Status::Ok(), AdapterResult{{protocol::StatusCode::INTERNAL_ERROR,
                                                        "mock adapter failure"}, simulated_results_}};
    if (!completion)
        return {{protocol::StatusCode::INVALID_ARGUMENT, "completion callback"}, std::nullopt};
    pending_[command.request_id] = std::move(completion);
    return {protocol::Status::Ok(), std::nullopt};
}

void MockServiceAdapter::cancel_request(protocol::RequestId request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_.count(request_id) != 0) ++cancellations_;
    // Keep the callback to let tests and real adapters prove that late completion is fenced.
}

void MockServiceAdapter::cancel_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancellations_ += pending_.size();
    pending_.clear();
}

void MockServiceAdapter::set_behavior(CommandType type, MockBehavior behavior) {
    std::lock_guard<std::mutex> lock(mutex_);
    behaviors_[type] = behavior;
}

std::size_t MockServiceAdapter::invocation_count(CommandType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = invocations_.find(type);
    return found == invocations_.end() ? 0 : found->second;
}

std::size_t MockServiceAdapter::cancellation_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancellations_;
}

protocol::Status MockServiceAdapter::complete_pending(protocol::RequestId request_id,
                                                       protocol::Status status) {
    AdapterCompletion completion;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = pending_.find(request_id);
        if (found == pending_.end())
            return {protocol::StatusCode::INVALID_STATE, "request is not pending"};
        completion = std::move(found->second);
        pending_.erase(found);
    }
    completion({std::move(status), simulated_results_});
    return protocol::Status::Ok();
}

}  // namespace cockpit::vehicle
