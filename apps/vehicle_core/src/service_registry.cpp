#include "cockpit/vehicle/service_registry.hpp"

#include "cockpit/vehicle/command.hpp"

namespace cockpit::vehicle {

ServiceRegistry::ServiceRegistry() {
    for (auto& state : states_) state = {ServiceHealth::UNKNOWN, StateSource::UNKNOWN, 0};
}

void ServiceRegistry::set(ServiceDomain domain, ServiceHealth health, StateSource source) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = states_.at(static_cast<std::size_t>(domain));
    if (state.health == health && state.source == source) return;
    state.health = health;
    state.source = source;
    ++state.sequence;
}

ServiceState ServiceRegistry::get(ServiceDomain domain) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return states_.at(static_cast<std::size_t>(domain));
}

std::array<ServiceState, static_cast<std::size_t>(ServiceDomain::COUNT)>
ServiceRegistry::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return states_;
}

bool ServiceRegistry::available(ServiceDomain domain) const {
    const auto health = get(domain).health;
    return health == ServiceHealth::ONLINE || health == ServiceHealth::DEGRADED;
}

ServiceDomain service_for(CommandType type) {
    switch (type) {
        case CommandType::CAMERA_SELECT:
        case CommandType::CAMERA_SNAPSHOT:
        case CommandType::RECORDING_START:
        case CommandType::RECORDING_STOP:
        case CommandType::RTSP_START:
        case CommandType::RTSP_STOP:
        case CommandType::MEDIA_PLAY:
        case CommandType::MEDIA_PAUSE:
        case CommandType::MEDIA_PREVIOUS:
        case CommandType::MEDIA_NEXT:
        case CommandType::MEDIA_STOP: return ServiceDomain::MEDIA;
        case CommandType::VOICE_SESSION_START:
        case CommandType::VOICE_SESSION_CANCEL: return ServiceDomain::VOICE;
        case CommandType::SIM_LED_SET:
        case CommandType::SIM_BUZZER_SET: return ServiceDomain::RTOS;
        case CommandType::QUERY_STATE: return ServiceDomain::SYSTEM;
    }
    return ServiceDomain::SYSTEM;
}

}  // namespace cockpit::vehicle
