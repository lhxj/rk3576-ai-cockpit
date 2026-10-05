#include "cockpit_ui/core_integration_runtime.h"

#include "cockpit_ui/vehicle_core_ui_backend.h"
#include "cockpit_ui/vision_ui_backend.h"

#ifdef COCKPIT_ENABLE_RKNN
#include "cockpit/infer/rknn_vision_backend.hpp"
#endif

#ifdef COCKPIT_ENABLE_V4L2_CAMERA
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#endif
#ifdef COCKPIT_ENABLE_MPP_RECORDING
#include "cockpit/media/mpp_h264_encoder.hpp"
#endif

#include <chrono>
#include <future>
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

media::MediaOperationResult submitAndWait(media::MediaService& service,
                                          media::MediaOperation operation) {
    auto promise = std::make_shared<std::promise<media::MediaOperationResult>>();
    auto future = promise->get_future();
    const auto accepted = service.submit(operation, [promise](auto result) {
        try { promise->set_value(std::move(result)); } catch (...) {}
    });
    if (!accepted.ok()) return {accepted};
    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
        return {{media::MediaStatusCode::Timeout, "vision operation timeout"}};
    return future.get();
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
    std::unique_ptr<media::ICameraCapture> capture_override,
    std::unique_ptr<media::IH264Encoder> encoder_override,
    std::unique_ptr<media::IFileRecordingSink> file_sink_override,
    std::unique_ptr<media::IRtspServer> rtsp_override,
    std::unique_ptr<infer::IVisionBackend> vision_override,
    std::unique_ptr<rpmsg::SensorTransport> sensor_override)
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
#ifdef COCKPIT_ENABLE_MPP_RECORDING
        if (!encoder_override)
            encoder_override = std::make_unique<media::MppH264Encoder>();
        if (!file_sink_override)
            file_sink_override = std::make_unique<media::FileRecordingSink>();
        if (!rtsp_override)
            rtsp_override = std::make_unique<media::RtspServer>();
#endif
        if (capture_override) {
            media::MediaServiceConfig service_config;
            service_config.capture = options_.camera;
            service_config.snapshot_directory = options_.snapshot_directory;
            service_config.recording_directory = options_.recording_directory;
            service_config.rtsp = options_.rtsp;
            media_service_ = std::make_shared<media::MediaService>(
                std::move(service_config), std::move(capture_override),
                std::make_shared<media::PreviewMailbox>(), std::move(encoder_override),
                std::move(file_sink_override), std::move(rtsp_override));
            real_media_ = std::make_shared<media::RealMediaServiceAdapter>(media_service_);
            media_adapter_ = real_media_;
        }
    }
    if (vision_override || options_.vision_backend == VisionBackendKind::RknnReal) {
        inference_scheduler_ = std::make_unique<infer::SerialInferenceScheduler>();
        if (vision_override) {
            vision_backend_ = std::move(vision_override);
        } else {
#ifdef COCKPIT_ENABLE_RKNN
            infer::RknnVisionBackendConfig config;
            config.model_path = options_.vision_model_path;
            config.model_name = "MobileNetV1 RK3576";
            vision_backend_ = std::make_unique<infer::RknnVisionBackend>(
                *inference_scheduler_, std::move(config));
#endif
        }
        if (vision_backend_) {
            infer::VisionRuntimeConfig config;
            config.target_fps = options_.vision_target_fps;
            config.queue_capacity = options_.vision_queue_capacity;
            vision_runtime_ = std::make_shared<infer::VisionRuntime>(*vision_backend_, config);
        }
    }
    core_ = std::make_unique<vehicle::VehicleCore>(
        makeConfig(clock_), vehicle::AdapterSet{media_adapter_, voice_, rtos_, system_},
        registry_, clock_);
    if (real_media_) {
        real_media_->set_runtime_state_callback(
            [this](vehicle::CommandType type, vehicle::AdapterResult result) {
                if (core_) (void)core_->report_runtime_result(type, std::move(result));
            });
    }
    sensor_transport_ = std::move(sensor_override);
    client_ = std::make_unique<vehicle::InProcessVehicleCoreClient>(*core_);
}

CoreIntegrationRuntime::~CoreIntegrationRuntime() { stop(); }

bool CoreIntegrationRuntime::start() {
    if (started_) return true;
    if (!media_adapter_ || !core_ || !client_) return false;
    if (options_.vision_backend != VisionBackendKind::None && !vision_runtime_)
        return false;
    if (media_service_) {
        const auto media_status = media_service_->start();
        if (!media_status.ok()) return false;
    }
    if (vision_runtime_) {
        if (!media_service_) return false;
        media_service_->set_vision_frame_callback(
            [runtime = vision_runtime_](std::shared_ptr<const media::CapturedFrame> frame) {
                if (!frame) return;
                infer::VisionFrame input;
                input.camera_id = frame->camera_id;
                input.width = frame->width;
                input.height = frame->height;
                input.pixel_format = frame->pixel_format;
                input.bytes_per_line = frame->bytes_per_line;
                input.bytes_used = frame->bytes_used;
                input.sequence = frame->sequence;
                input.stream_epoch = frame->stream_epoch;
                input.capture_timestamp_ns = frame->capture_timestamp_ns;
                input.dequeue_steady_timestamp_ns = frame->dequeue_steady_timestamp_ns;
                const auto* payload = &frame->payload;
                input.payload = std::shared_ptr<const std::vector<std::uint8_t>>(
                    std::move(frame), payload);
                (void)runtime->submit(std::move(input));
            });
        const auto vision_status = vision_runtime_->start();
        if (!vision_status.ok()) {
            media_service_->set_vision_frame_callback({});
            media_service_->stop();
            return false;
        }
        const auto consumer = submitAndWait(*media_service_, media::MediaOperation::VisionStart);
        if (!consumer.status.ok()) {
            (void)vision_runtime_->stop();
            media_service_->set_vision_frame_callback({});
            media_service_->stop();
            return false;
        }
        registry_->set(vehicle::ServiceDomain::INFER, vehicle::ServiceHealth::ONLINE,
                       vehicle::StateSource::RUNTIME);
    }
    const auto status = core_->start();
    started_ = status.ok();
    if (started_ && (sensor_transport_ || !options_.sensor_device_path.empty())) {
        try {
        sensor_runtime_ = std::make_unique<rpmsg::SensorRuntime>(
            sensor_transport_ ? std::move(sensor_transport_) : rpmsg::make_device_transport(options_.sensor_device_path), core_->boot_epoch(),
            [this](const vehicle::SensorState& state) { (void)core_->report_sensor_state(state); });
        if (!sensor_runtime_->start()) { sensor_runtime_.reset(); core_->stop(); started_ = false; }
        } catch (...) { sensor_runtime_.reset(); core_->stop(); started_ = false; }
    }
    if (!started_) {
        if (media_service_ && vision_runtime_)
            (void)submitAndWait(*media_service_, media::MediaOperation::VisionStop);
        if (vision_runtime_ && vision_runtime_->running()) (void)vision_runtime_->stop();
        if (media_service_) {
            media_service_->set_vision_frame_callback({});
            media_service_->stop();
        }
    }
    return started_;
}

void CoreIntegrationRuntime::stop() {
    if (!started_ && !media_service_) return;
    if (sensor_runtime_) { sensor_runtime_->stop(); sensor_runtime_.reset(); }
    if (core_) core_->stop();
    if (media_service_ && vision_runtime_)
        (void)submitAndWait(*media_service_, media::MediaOperation::VisionStop);
    if (vision_runtime_ && vision_runtime_->running()) (void)vision_runtime_->stop();
    if (media_service_) media_service_->set_vision_frame_callback({});
    if (media_service_) media_service_->stop();
    if (vision_runtime_)
        registry_->set(vehicle::ServiceDomain::INFER, vehicle::ServiceHealth::OFFLINE,
                       vehicle::StateSource::RUNTIME);
    started_ = false;
}

std::unique_ptr<IUiBackend> CoreIntegrationRuntime::makeUiBackend() {
    std::unique_ptr<IUiBackend> backend =
        std::make_unique<VehicleCoreUiBackend>(*client_, clock_);
    if (vision_runtime_)
        backend = std::make_unique<VisionUiBackend>(std::move(backend), vision_runtime_);
    return backend;
}

}  // namespace cockpit::ui
