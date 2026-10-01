#pragma once

#include "cockpit/vehicle/command.hpp"
#include "cockpit/vehicle/state.hpp"

#include <array>
#include <mutex>

namespace cockpit::vehicle {

class ServiceRegistry {
public:
    ServiceRegistry();
    void set(ServiceDomain domain, ServiceHealth health, StateSource source);
    ServiceState get(ServiceDomain domain) const;
    std::array<ServiceState, static_cast<std::size_t>(ServiceDomain::COUNT)> snapshot() const;
    bool available(ServiceDomain domain) const;

private:
    mutable std::mutex mutex_;
    std::array<ServiceState, static_cast<std::size_t>(ServiceDomain::COUNT)> states_{};
};

ServiceDomain service_for(CommandType type);

}  // namespace cockpit::vehicle
