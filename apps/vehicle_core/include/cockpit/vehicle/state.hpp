#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>

namespace cockpit::vehicle {

enum class StateSource { UNKNOWN, RUNTIME, HISTORICAL, MOCK };
enum class StateCondition {
    UNKNOWN, OFFLINE, STARTING, ONLINE, DEGRADED, ERROR, SIMULATED, STOPPING
};
enum class CameraAvailability { UNKNOWN, UNAVAILABLE, AVAILABLE };
enum class CameraSelection { NONE, FRONT, REAR };
enum class RecordingState { STOPPED, STARTING, RECORDING, STOPPING, ERROR };
enum class PreviewState { STOPPED, STARTING, STREAMING, STOPPING, ERROR };
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

enum class SensorDataCondition { NO_DATA, VALID, STALE, OFFLINE, ERROR };
struct SensorState {
    StateSource source{StateSource::UNKNOWN};
    bool unsubscribe_confirmed{false}, subscription_active{false};
    bool rtos_online{false}, rpmsg_online{false}, mpu_available{false}, has_value{false};
    SensorDataCondition data{SensorDataCondition::NO_DATA};
    std::array<std::int16_t,3> accel_raw{}, gyro_raw{};
    std::int16_t chip_temp_raw{0};
    std::array<double,3> accel_g{}, gyro_dps{};
    double chip_temp_c{0};
    std::uint64_t remote_epoch{0}, subscription_id{0}, sample_seq{0}, publish_seq{0};
    std::uint64_t m0_ms{0}, rx_ms{0}, age_ms{0}, generation{0};
    std::uint64_t duplicate{0}, out_of_order{0}, sequence_gaps{0}, protocol_errors{0}, old_packets{0};
    std::uint64_t sample_errors{0}, latest_overwrites{0}, send_failures{0}, control_drops{0};
    std::uint64_t linux_sample_overwrites{0}, linux_control_drops{0}, linux_malformed{0}, linux_send_failures{0};
    std::uint16_t error{0};
    std::uint32_t config_id{0};
    std::uint8_t accel_fs{0}, gyro_fs{0}, dlpf{3}, divider{49}, power{1}, m0_time_unit{1};
    std::uint16_t internal_odr_hz{20}, read_target_hz{20}, publish_cap_hz{20};
};

struct VehicleState {
    std::uint64_t revision{0};
    StateValue<CameraAvailability> front_camera;
    StateValue<CameraAvailability> rear_camera;
    StateValue<CameraSelection> selected_camera;
    StateValue<PreviewState> preview;
    StateValue<RecordingState> recording;
    StateValue<BinaryState> rtsp;
    StateValue<MediaState> media;
    StateValue<BinaryState> audio;
    StateValue<VoiceState> voice;
    StateValue<BinaryState> vision;
    StateValue<BinaryState> language_model;
    StateValue<BinaryState> rtos;
    StateValue<BinaryState> sensor;
    SensorState sensor_state;
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
