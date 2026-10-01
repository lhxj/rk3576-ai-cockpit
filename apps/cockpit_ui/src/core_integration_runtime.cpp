#include "cockpit_ui/core_integration_runtime.h"

#include "cockpit_ui/vehicle_core_ui_backend.h"

#include <chrono>
#include <utility>

namespace cockpit::ui {
namespace {

std::shared_ptr<vehicle::ServiceRegistry> makeRegistry(CoreDemoProfile profile) {
    auto registry = std::make_shared<vehicle::ServiceRegistry>();
    registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::INFER, vehicle::ServiceHealth::OFFLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::RTOS,
                  profile == CoreDemoProfile::RtosOffline ? vehicle::ServiceHealth::OFFLINE
                                                           : vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    return registry;
}

vehicle::VehicleCoreConfig makeConfig(const std::shared_ptr<vehicle::SystemClock>& clock) {
    return {clock->now_ms(), 32, 32, 128, std::chrono::milliseconds(5)};
}

}  // namespace

bool parseCoreDemoProfile(std::string_view name, CoreDemoProfile& profile) {
    if (name == "normal") {
        profile = CoreDemoProfile::Normal;
    } else if (name == "media-failure") {
        profile = CoreDemoProfile::MediaFailure;
    } else if (name == "media-timeout") {
        profile = CoreDemoProfile::MediaTimeout;
    } else if (name == "rtos-offline") {
        profile = CoreDemoProfile::RtosOffline;
    } else {
        return false;
    }
    return true;
}

std::string_view coreDemoProfileName(CoreDemoProfile profile) noexcept {
    switch (profile) {
    case CoreDemoProfile::Normal:
        return "normal";
    case CoreDemoProfile::MediaFailure:
        return "media-failure";
    case CoreDemoProfile::MediaTimeout:
        return "media-timeout";
    case CoreDemoProfile::RtosOffline:
        return "rtos-offline";
    }
    return "normal";
}

CoreIntegrationRuntime::CoreIntegrationRuntime(CoreDemoProfile profile)
    : profile_(profile), clock_(std::make_shared<vehicle::SystemClock>()),
      registry_(makeRegistry(profile)), media_(std::make_shared<vehicle::MockMediaAdapter>()),
      voice_(std::make_shared<vehicle::MockVoiceAdapter>()),
      rtos_(std::make_shared<vehicle::MockRtosAdapter>()),
      system_(std::make_shared<vehicle::MockSystemAdapter>()),
      core_(makeConfig(clock_), {media_, voice_, rtos_, system_}, registry_, clock_), client_(core_) {
    if (profile_ == CoreDemoProfile::MediaFailure)
        media_->set_behavior(vehicle::CommandType::RECORDING_START,
                             vehicle::MockBehavior::FAILURE);
    if (profile_ == CoreDemoProfile::MediaTimeout)
        media_->set_behavior(vehicle::CommandType::RECORDING_START,
                             vehicle::MockBehavior::TIMEOUT);
}

CoreIntegrationRuntime::~CoreIntegrationRuntime() { stop(); }

bool CoreIntegrationRuntime::start() {
    if (started_) return true;
    const auto status = core_.start();
    started_ = status.ok();
    return started_;
}

void CoreIntegrationRuntime::stop() {
    if (!started_) return;
    core_.stop();
    started_ = false;
}

std::unique_ptr<IUiBackend> CoreIntegrationRuntime::makeUiBackend() {
    return std::make_unique<VehicleCoreUiBackend>(client_, clock_);
}

}  // namespace cockpit::ui
