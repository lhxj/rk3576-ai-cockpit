#pragma once

#include "cockpit/ipc/transport.hpp"
#include "cockpit/vehicle/core.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cockpit::vehicle {

class IVehicleCoreClient {
public:
    virtual ~IVehicleCoreClient() = default;
    virtual CommandSubmission send_command(const VehicleCommand& command) = 0;
    virtual VehicleState get_snapshot() const = 0;
    virtual protocol::Status subscribe_state(StateCallback callback) = 0;
};

class InProcessVehicleCoreClient final : public IVehicleCoreClient {
public:
    explicit InProcessVehicleCoreClient(VehicleCore& core) : core_(core) {}
    CommandSubmission send_command(const VehicleCommand& command) override;
    VehicleState get_snapshot() const override;
    protocol::Status subscribe_state(StateCallback callback) override;
private:
    VehicleCore& core_;
};

// Host-only serialization/transport integration. It exercises the same explicit wire
// codec used by future IPC clients, but remains in one process and opens no socket.
class LoopbackVehicleCoreClient final : public IVehicleCoreClient {
public:
    LoopbackVehicleCoreClient(VehicleCore& core, std::size_t queue_capacity,
                              std::chrono::milliseconds response_timeout = std::chrono::seconds(1));
    ~LoopbackVehicleCoreClient() override;
    protocol::Status start();
    void stop();
    CommandSubmission send_command(const VehicleCommand& command) override;
    VehicleState get_snapshot() const override;
    protocol::Status subscribe_state(StateCallback callback) override;

private:
    struct Waiter {
        std::mutex mutex;
        std::condition_variable ready;
        bool completed{false};
        CommandSubmission submission;
    };
    void receive(const protocol::Message& message);
    VehicleCore& core_;
    ipc::InMemoryTransport transport_;
    const std::chrono::milliseconds response_timeout_;
    mutable std::mutex mutex_;
    std::unordered_map<protocol::RequestId, std::deque<std::shared_ptr<Waiter>>> waiters_;
    bool started_{false};
};

}  // namespace cockpit::vehicle
