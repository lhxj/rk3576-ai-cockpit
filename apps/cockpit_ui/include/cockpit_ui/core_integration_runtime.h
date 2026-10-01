#pragma once

#include "cockpit/media/camera_capture.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit_ui/ui_backend.h"

#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace cockpit::ui {

enum class CoreDemoProfile { Normal, MediaFailure, MediaTimeout, RtosOffline };
enum class MediaBackendKind { Mock, Cam0Real };

struct CoreIntegrationRuntimeOptions {
    CoreDemoProfile profile{CoreDemoProfile::Normal};
    MediaBackendKind media_backend{MediaBackendKind::Mock};
    media::CameraCaptureConfig camera;
    std::string snapshot_directory;
    std::string recording_directory;
};

[[nodiscard]] bool parseCoreDemoProfile(std::string_view name, CoreDemoProfile& profile);
[[nodiscard]] std::string_view coreDemoProfileName(CoreDemoProfile profile) noexcept;

class CoreIntegrationRuntime {
public:
    explicit CoreIntegrationRuntime(CoreDemoProfile profile = CoreDemoProfile::Normal);
    CoreIntegrationRuntime(CoreIntegrationRuntimeOptions options,
                           std::unique_ptr<media::ICameraCapture> capture_override = {},
                           std::unique_ptr<media::IMediaRecorder> recorder_override = {});
    ~CoreIntegrationRuntime();

    CoreIntegrationRuntime(const CoreIntegrationRuntime&) = delete;
    CoreIntegrationRuntime& operator=(const CoreIntegrationRuntime&) = delete;

    bool start();
    void stop();
    [[nodiscard]] std::unique_ptr<IUiBackend> makeUiBackend();

    [[nodiscard]] vehicle::VehicleCore& core() { return *core_; }
    [[nodiscard]] vehicle::InProcessVehicleCoreClient& client() { return *client_; }
    [[nodiscard]] std::shared_ptr<vehicle::MockMediaAdapter> mediaAdapter() const {
        return mock_media_;
    }
    [[nodiscard]] std::shared_ptr<media::RealMediaServiceAdapter> realMediaAdapter() const {
        return real_media_;
    }
    [[nodiscard]] std::shared_ptr<media::MediaService> mediaService() const {
        return media_service_;
    }
    [[nodiscard]] std::shared_ptr<media::PreviewMailbox> previewMailbox() const {
        return media_service_ ? media_service_->preview_mailbox() : nullptr;
    }
    [[nodiscard]] std::shared_ptr<vehicle::MockVoiceAdapter> voiceAdapter() const {
        return voice_;
    }
    [[nodiscard]] std::shared_ptr<vehicle::MockRtosAdapter> rtosAdapter() const {
        return rtos_;
    }
    [[nodiscard]] MediaBackendKind mediaBackendKind() const { return options_.media_backend; }

private:
    CoreIntegrationRuntimeOptions options_;
    std::shared_ptr<vehicle::SystemClock> clock_;
    std::shared_ptr<vehicle::ServiceRegistry> registry_;
    std::shared_ptr<vehicle::IServiceAdapter> media_adapter_;
    std::shared_ptr<vehicle::MockMediaAdapter> mock_media_;
    std::shared_ptr<media::RealMediaServiceAdapter> real_media_;
    std::shared_ptr<media::MediaService> media_service_;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice_;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos_;
    std::shared_ptr<vehicle::MockSystemAdapter> system_;
    std::unique_ptr<vehicle::VehicleCore> core_;
    std::unique_ptr<vehicle::InProcessVehicleCoreClient> client_;
    bool started_{false};
};

}  // namespace cockpit::ui
