#pragma once

#include "cockpit_ui/ui_backend.h"

#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"

#include <memory>
#include <string_view>

namespace cockpit::ui {

enum class CoreDemoProfile { Normal, MediaFailure, MediaTimeout, RtosOffline };

[[nodiscard]] bool parseCoreDemoProfile(std::string_view name, CoreDemoProfile& profile);
[[nodiscard]] std::string_view coreDemoProfileName(CoreDemoProfile profile) noexcept;

class CoreIntegrationRuntime {
public:
    explicit CoreIntegrationRuntime(CoreDemoProfile profile = CoreDemoProfile::Normal);
    ~CoreIntegrationRuntime();

    CoreIntegrationRuntime(const CoreIntegrationRuntime&) = delete;
    CoreIntegrationRuntime& operator=(const CoreIntegrationRuntime&) = delete;

    bool start();
    void stop();
    [[nodiscard]] std::unique_ptr<IUiBackend> makeUiBackend();

    [[nodiscard]] vehicle::VehicleCore& core() { return core_; }
    [[nodiscard]] vehicle::InProcessVehicleCoreClient& client() { return client_; }
    [[nodiscard]] std::shared_ptr<vehicle::MockMediaAdapter> mediaAdapter() const {
        return media_;
    }
    [[nodiscard]] std::shared_ptr<vehicle::MockVoiceAdapter> voiceAdapter() const {
        return voice_;
    }
    [[nodiscard]] std::shared_ptr<vehicle::MockRtosAdapter> rtosAdapter() const {
        return rtos_;
    }

private:
    CoreDemoProfile profile_;
    std::shared_ptr<vehicle::SystemClock> clock_;
    std::shared_ptr<vehicle::ServiceRegistry> registry_;
    std::shared_ptr<vehicle::MockMediaAdapter> media_;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice_;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos_;
    std::shared_ptr<vehicle::MockSystemAdapter> system_;
    vehicle::VehicleCore core_;
    vehicle::InProcessVehicleCoreClient client_;
    bool started_{false};
};

}  // namespace cockpit::ui
