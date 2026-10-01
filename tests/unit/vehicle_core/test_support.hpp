#pragma once

#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"

#include <chrono>
#include <memory>
#include <thread>

namespace cockpit::vehicle::test {

struct Fixture {
    explicit Fixture(std::size_t queue_capacity = 16, bool auto_start = true)
        : clock(std::make_shared<FakeClock>(1000)), registry(std::make_shared<ServiceRegistry>()),
          media(std::make_shared<MockMediaAdapter>()), voice(std::make_shared<MockVoiceAdapter>()),
          rtos(std::make_shared<MockRtosAdapter>()), system(std::make_shared<MockSystemAdapter>()),
          core({77, queue_capacity, 16, 128, std::chrono::milliseconds(1)},
               {media, voice, rtos, system}, registry, clock), client(core) {
        registry->set(ServiceDomain::MEDIA, ServiceHealth::ONLINE, StateSource::MOCK);
        registry->set(ServiceDomain::VOICE, ServiceHealth::ONLINE, StateSource::MOCK);
        registry->set(ServiceDomain::INFER, ServiceHealth::OFFLINE, StateSource::MOCK);
        registry->set(ServiceDomain::RTOS, ServiceHealth::ONLINE, StateSource::MOCK);
        registry->set(ServiceDomain::SYSTEM, ServiceHealth::ONLINE, StateSource::MOCK);
        if (auto_start) start_status = core.start();
    }
    ~Fixture() { core.stop(); }

    VehicleCommand command(protocol::RequestId id, CommandType type,
                           CommandParameters parameters = {}) const {
        VehicleCommand command;
        command.request_id = id;
        command.boot_epoch = 77;
        command.deadline_ms = clock->now_ms() + 100;
        command.source = CommandSource::TEST;
        command.command_type = type;
        command.parameters = std::move(parameters);
        return command;
    }

    std::shared_ptr<FakeClock> clock;
    std::shared_ptr<ServiceRegistry> registry;
    std::shared_ptr<MockMediaAdapter> media;
    std::shared_ptr<MockVoiceAdapter> voice;
    std::shared_ptr<MockRtosAdapter> rtos;
    std::shared_ptr<MockSystemAdapter> system;
    VehicleCore core;
    InProcessVehicleCoreClient client;
    protocol::Status start_status;
};

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::yield();
    }
    return predicate();
}

inline bool ready(const std::shared_future<CommandResult>& result) {
    return result.valid() && result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

}  // namespace cockpit::vehicle::test
