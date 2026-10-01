#include "cockpit_ui/core_integration_runtime.h"

#include "cockpit_ui/vehicle_core_ui_backend.h"

#ifdef COCKPIT_ENABLE_V4L2_CAMERA
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#endif

#include <chrono>
#include <utility>

namespace cockpit::ui {
namespace {

std::shared_ptr<vehicle::ServiceRegistry> makeRegistry(
    const CoreIntegrationRuntimeOptions& options) {
    auto registry = std::make_shared<vehicle::ServiceRegistry>();
    registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                  options.media_backend == MediaBackendKind::Cam0Real
                      ? vehicle::StateSource::RUNTIME
                      : vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::INFER, vehicle::ServiceHealth::OFFLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::RTOS,
                  options.profile == CoreDemoProfile::RtosOffline
                      ? vehicle::ServiceHealth::OFFLINE
                      : vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                  vehicle::StateSource::MOCK);
    return registry;
}

vehicle::VehicleCoreConfig makeConfig(const std::shared_ptr<vehicle::SystemClock>& clock) {
    return {clock->now_ms(), 32, 32, 128, std::chrono::milliseconds(5)};
}

CoreIntegrationRuntimeOptions mockOptions(CoreDemoProfile profile) {
    CoreIntegrationRuntimeOptions options;
    options.profile = profile;
    options.media_backend = MediaBackendKind::Mock;
    return options;
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
    case CoreDemoProfile::Normal: return "normal";
    case CoreDemoProfile::MediaFailure: return "media-failure";
    case CoreDemoProfile::MediaTimeout: return "media-timeout";
    case CoreDemoProfile::RtosOffline: return "rtos-offline";
    }
    return "normal";
}

CoreIntegrationRuntime::CoreIntegrationRuntime(CoreDemoProfile profile)
    : CoreIntegrationRuntime(mockOptions(profile)) {}

CoreIntegrationRuntime::CoreIntegrationRuntime(
    CoreIntegrationRuntimeOptions options,
    std::unique_ptr<media::ICameraCapture> capture_override)
    : options_(std::move(options)), clock_(std::make_shared<vehicle::SystemClock>()),
      registry_(makeRegistry(options_)), voice_(std::make_shared<vehicle::MockVoiceAdapter>()),
      rtos_(std::make_shared<vehicle::MockRtosAdapter>()),
      system_(std::make_shared<vehicle::MockSystemAdapter>()) {
    if (options_.media_backend == MediaBackendKind::Mock) {
        mock_media_ = std::make_shared<vehicle::MockMediaAdapter>();
        media_adapter_ = mock_media_;
        if (options_.profile == CoreDemoProfile::MediaFailure)
            mock_media_->set_behavior(vehicle::CommandType::RECORDING_START,
                                      vehicle::MockBehavior::FAILURE);
        if (options_.profile == CoreDemoProfile::MediaTimeout)
            mock_media_->set_behavior(vehicle::CommandType::RECORDING_START,
                                      vehicle::MockBehavior::TIMEOUT);
    } else {
#ifdef COCKPIT_ENABLE_V4L2_CAMERA
        if (!capture_override)
            capture_override = std::make_unique<media::V4l2MplaneCameraCapture>();
#endif
        if (capture_override) {
            media::MediaServiceConfig service_config;
            service_config.capture = options_.camera;
            service_config.snapshot_directory = options_.snapshot_directory;
            media_service_ = std::make_shared<media::MediaService>(
                std::move(service_config), std::move(capture_override));
            real_media_ = std::make_shared<media::RealMediaServiceAdapter>(media_service_);
            media_adapter_ = real_media_;
        }
    }
    core_ = std::make_unique<vehicle::VehicleCore>(
        makeConfig(clock_), vehicle::AdapterSet{media_adapter_, voice_, rtos_, system_},
        registry_, clock_);
    client_ = std::make_unique<vehicle::InProcessVehicleCoreClient>(*core_);
}

CoreIntegrationRuntime::~CoreIntegrationRuntime() { stop(); }

bool CoreIntegrationRuntime::start() {
    if (started_) return true;
    if (!media_adapter_ || !core_ || !client_) return false;
    if (media_service_) {
        const auto media_status = media_service_->start();
        if (!media_status.ok()) return false;
    }
    const auto status = core_->start();
    started_ = status.ok();
    if (!started_ && media_service_) media_service_->stop();
    return started_;
}

void CoreIntegrationRuntime::stop() {
    if (!started_ && !media_service_) return;
    if (core_) core_->stop();
    if (media_service_) media_service_->stop();
    started_ = false;
}

std::unique_ptr<IUiBackend> CoreIntegrationRuntime::makeUiBackend() {
    return std::make_unique<VehicleCoreUiBackend>(*client_, clock_);
}

}  // namespace cockpit::ui
