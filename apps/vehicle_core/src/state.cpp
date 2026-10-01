#include "cockpit/vehicle/state.hpp"

#include <utility>

namespace cockpit::vehicle {

StateStore::StateStore(VehicleState initial) : state_(std::move(initial)) {}

VehicleState StateStore::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool StateStore::update(const std::function<bool(VehicleState&)>& writer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!writer || !writer(state_)) return false;
    ++state_.revision;
    return true;
}

}  // namespace cockpit::vehicle
