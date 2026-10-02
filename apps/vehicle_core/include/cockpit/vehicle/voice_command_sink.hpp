#pragma once

#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/voice/voice.hpp"

#include <mutex>
#include <optional>

namespace cockpit::vehicle {

// Adapts the existing voice safety boundary. A session must be activated by the
// voice orchestrator with a finite deadline before a candidate can enter the core.
class VehicleCommandSinkAdapter final : public voice::IVoiceSessionCommandSink {
public:
    VehicleCommandSinkAdapter(IVehicleCoreClient& client, std::shared_ptr<IClock> clock,
                              protocol::BootEpoch boot_epoch);
    protocol::Status activate_session(voice::SessionToken token,
                                      protocol::Deadline deadline_ms) override;
    protocol::Status cancel_session(voice::SessionToken token) override;
    protocol::Status submit_candidate(const voice::CandidateAction& action) override;
    std::optional<CommandSubmission> last_submission() const;

private:
    IVehicleCoreClient& client_;
    std::shared_ptr<IClock> clock_;
    const protocol::BootEpoch boot_epoch_;
    mutable std::mutex mutex_;
    voice::SessionToken active_;
    protocol::Deadline deadline_ms_{0};
    bool active_valid_{false};
    bool cancelled_{false};
    std::optional<CommandSubmission> last_submission_;
};

}  // namespace cockpit::vehicle
