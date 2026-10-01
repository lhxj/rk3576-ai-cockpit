#pragma once

#include "cockpit_ui/ui_backend.h"

#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace cockpit::ui {

[[nodiscard]] UiState mapVehicleState(const vehicle::VehicleState& state);

class RevisionedStateProjector {
public:
    bool apply(const vehicle::VehicleState& state, UiState& projected);
    [[nodiscard]] std::uint64_t lastRevision() const noexcept { return last_revision_; }

private:
    bool initialized_{false};
    std::uint64_t last_revision_{0};
};

struct VehicleCoreUiBackendOptions {
    std::size_t pending_capacity{64};
    protocol::Deadline command_timeout_ms{2000};
};

class VehicleCoreUiBackend final : public IUiBackend {
public:
    VehicleCoreUiBackend(vehicle::IVehicleCoreClient& client,
                         std::shared_ptr<vehicle::IClock> clock,
                         VehicleCoreUiBackendOptions options = {});
    ~VehicleCoreUiBackend() override;

    VehicleCoreUiBackend(const VehicleCoreUiBackend&) = delete;
    VehicleCoreUiBackend& operator=(const VehicleCoreUiBackend&) = delete;

    bool start() override;
    void stop() override;
    [[nodiscard]] UiState currentState() const override;
    UiResult submit(const UiRequest& request) override;
    void setStateCallback(StateCallback callback) override;
    void setResultCallback(ResultCallback callback) override;

private:
    struct CallbackGate {
        std::mutex mutex;
        VehicleCoreUiBackend* owner{nullptr};
    };

    struct PendingResult {
        std::uint64_t request_id{0};
        UiCommand command{UiCommand::MediaStop};
        std::shared_future<vehicle::CommandResult> future;
    };

    vehicle::VehicleCommand makeCommand(std::uint64_t request_id, const UiRequest& request);
    void receiveState(const vehicle::VehicleState& state);
    void resultLoop();
    void complete(PendingResult pending);
    [[nodiscard]] UiState snapshotWithPendingLocked() const;
    void publishState(const UiState& state, const StateCallback& callback) const;
    void publishResult(const UiResult& result, const ResultCallback& callback) const;

    vehicle::IVehicleCoreClient& client_;
    std::shared_ptr<vehicle::IClock> clock_;
    const VehicleCoreUiBackendOptions options_;
    std::shared_ptr<CallbackGate> callback_gate_;
    std::mutex submit_mutex_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    UiState canonical_state_;
    RevisionedStateProjector projector_;
    std::vector<PendingResult> pending_;
    StateCallback state_callback_;
    ResultCallback result_callback_;
    std::thread result_worker_;
    std::atomic<std::uint64_t> next_request_id_{1};
    protocol::SessionId voice_session_id_{0};
    bool started_{false};
    bool stopping_{false};
};

}  // namespace cockpit::ui
