#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>

namespace cockpit::vehicle {

enum class StateSource { UNKNOWN, RUNTIME, HISTORICAL, MOCK };
enum class StateCondition { UNKNOWN, OFFLINE, STARTING, ONLINE, DEGRADED, ERROR, SIMULATED };
enum class CameraAvailability { UNKNOWN, UNAVAILABLE, AVAILABLE };
enum class CameraSelection { NONE, FRONT, REAR };
enum class RecordingState { STOPPED, STARTING, RECORDING, STOPPING, ERROR };
enum class BinaryState { OFF, ON };
enum class MediaState { STOPPED, PLAYING, PAUSED, ERROR };
enum class VoiceState { IDLE, STARTING, ACTIVE, CANCELLING, ERROR };
enum class ServiceDomain : std::size_t { MEDIA = 0, VOICE, INFER, RTOS, SYSTEM, COUNT };
enum class ServiceHealth { UNKNOWN, STARTING, ONLINE, DEGRADED, OFFLINE, ERROR };

template <typename T>
struct StateValue {
    T value{};
    StateCondition condition{StateCondition::UNKNOWN};
    StateSource source{StateSource::UNKNOWN};
    std::uint64_t sequence{0};
};

struct ServiceState {
    ServiceHealth health{ServiceHealth::UNKNOWN};
    StateSource source{StateSource::UNKNOWN};
    std::uint64_t sequence{0};
};

inline bool operator==(const ServiceState& left, const ServiceState& right) {
    return left.health == right.health && left.source == right.source &&
           left.sequence == right.sequence;
}

struct VehicleState {
    std::uint64_t revision{0};
    StateValue<CameraAvailability> front_camera;
    StateValue<CameraAvailability> rear_camera;
    StateValue<CameraSelection> selected_camera;
    StateValue<RecordingState> recording;
    StateValue<BinaryState> rtsp;
    StateValue<MediaState> media;
    StateValue<BinaryState> audio;
    StateValue<VoiceState> voice;
    StateValue<BinaryState> vision;
    StateValue<BinaryState> language_model;
    StateValue<BinaryState> rtos;
    StateValue<BinaryState> sensor;
    StateValue<BinaryState> simulated_led;
    StateValue<BinaryState> simulated_buzzer;
    StateValue<BinaryState> wifi;
    std::array<ServiceState, static_cast<std::size_t>(ServiceDomain::COUNT)> services{};
};

using StateCallback = std::function<void(const VehicleState&)>;

class StateStore {
public:
    explicit StateStore(VehicleState initial);
    VehicleState snapshot() const;
    bool update(const std::function<bool(VehicleState&)>& writer);

private:
    mutable std::mutex mutex_;
    VehicleState state_;
};

template <typename T>
bool set_state_value(VehicleState& state, StateValue<T>& field, T value,
                     StateCondition condition, StateSource source) {
    if (field.value == value && field.condition == condition && field.source == source) return false;
    field.value = value;
    field.condition = condition;
    field.source = source;
    field.sequence = state.revision + 1;
    return true;
}

}  // namespace cockpit::vehicle
