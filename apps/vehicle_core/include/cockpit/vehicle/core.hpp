#pragma once

#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/command.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"
#include "cockpit/vehicle/state.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace cockpit::vehicle {

struct VehicleCoreConfig {
    protocol::BootEpoch boot_epoch{0};
    std::size_t command_queue_capacity{32};
    std::size_t completion_queue_capacity{32};
    std::size_t recent_request_capacity{128};
    std::chrono::milliseconds poll_interval{10};
};

class VehicleCore {
public:
    VehicleCore(VehicleCoreConfig config, AdapterSet adapters,
                std::shared_ptr<ServiceRegistry> registry,
                std::shared_ptr<IClock> clock);
    ~VehicleCore();
    VehicleCore(const VehicleCore&) = delete;
    VehicleCore& operator=(const VehicleCore&) = delete;

    protocol::Status start();
    void stop();
    bool running() const;
    CommandSubmission send_command(const VehicleCommand& command);
    VehicleState get_snapshot() const;
    protocol::Status subscribe_state(StateCallback callback);
    protocol::Status set_service_health(ServiceDomain domain, ServiceHealth health,
                                        StateSource source = StateSource::MOCK);
    protocol::Status poll_deadlines();
    protocol::Status report_sensor_state(SensorState state);
    protocol::Status report_runtime_result(CommandType type, AdapterResult result);
    protocol::BootEpoch boot_epoch() const;
    std::uint64_t ignored_late_results() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpit::vehicle
