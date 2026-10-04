#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/vehicle_core_ui_backend.h"
#include "cockpit_ui/vision_ui_backend.h"

#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/media/fake_camera_capture.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(expression)                                                                    \
    do {                                                                                     \
        if (!(expression)) {                                                                 \
            std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #expression << '\n';   \
            return 1;                                                                        \
        }                                                                                    \
    } while (false)

namespace {

template <typename Predicate>
bool waitUntil(Predicate predicate,
               std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::yield();
    }
    return predicate();
}

struct Fixture {
    Fixture()
        : clock(std::make_shared<cockpit::vehicle::FakeClock>(1000)),
          registry(std::make_shared<cockpit::vehicle::ServiceRegistry>()),
          media(std::make_shared<cockpit::vehicle::MockMediaAdapter>()),
          voice(std::make_shared<cockpit::vehicle::MockVoiceAdapter>()),
          rtos(std::make_shared<cockpit::vehicle::MockRtosAdapter>()),
          system(std::make_shared<cockpit::vehicle::MockSystemAdapter>()),
          core({77, 16, 16, 128, std::chrono::milliseconds(1)},
               {media, voice, rtos, system}, registry, clock),
          client(core), backend(client, clock) {
        registry->set(cockpit::vehicle::ServiceDomain::MEDIA,
                      cockpit::vehicle::ServiceHealth::ONLINE,
                      cockpit::vehicle::StateSource::MOCK);
        registry->set(cockpit::vehicle::ServiceDomain::VOICE,
                      cockpit::vehicle::ServiceHealth::ONLINE,
                      cockpit::vehicle::StateSource::MOCK);
        registry->set(cockpit::vehicle::ServiceDomain::INFER,
                      cockpit::vehicle::ServiceHealth::OFFLINE,
                      cockpit::vehicle::StateSource::MOCK);
        registry->set(cockpit::vehicle::ServiceDomain::RTOS,
                      cockpit::vehicle::ServiceHealth::ONLINE,
                      cockpit::vehicle::StateSource::MOCK);
        registry->set(cockpit::vehicle::ServiceDomain::SYSTEM,
                      cockpit::vehicle::ServiceHealth::ONLINE,
                      cockpit::vehicle::StateSource::MOCK);
    }

    ~Fixture() {
        backend.stop();
        core.stop();
    }

    std::shared_ptr<cockpit::vehicle::FakeClock> clock;
    std::shared_ptr<cockpit::vehicle::ServiceRegistry> registry;
    std::shared_ptr<cockpit::vehicle::MockMediaAdapter> media;
    std::shared_ptr<cockpit::vehicle::MockVoiceAdapter> voice;
    std::shared_ptr<cockpit::vehicle::MockRtosAdapter> rtos;
    std::shared_ptr<cockpit::vehicle::MockSystemAdapter> system;
    cockpit::vehicle::VehicleCore core;
    cockpit::vehicle::InProcessVehicleCoreClient client;
    cockpit::ui::VehicleCoreUiBackend backend;
};

}  // namespace

int main() {
    using namespace cockpit;
    using namespace cockpit::ui;
    using namespace cockpit::vehicle;

    VehicleState source;
    source.revision = 12;
    source.front_camera = {CameraAvailability::AVAILABLE, StateCondition::ONLINE,
                           vehicle::StateSource::MOCK, 10};
    source.rear_camera = {CameraAvailability::UNAVAILABLE, StateCondition::OFFLINE,
                          vehicle::StateSource::MOCK, 10};
    source.selected_camera = {CameraSelection::FRONT, StateCondition::ONLINE,
                              vehicle::StateSource::MOCK, 10};
    source.recording = {RecordingState::RECORDING, StateCondition::ONLINE,
                        vehicle::StateSource::MOCK, 11};
    source.rtsp = {BinaryState::ON, StateCondition::ONLINE, vehicle::StateSource::MOCK, 11};
    source.media = {MediaState::PLAYING, StateCondition::ONLINE, vehicle::StateSource::MOCK, 11};
    source.voice = {VoiceState::ACTIVE, StateCondition::ONLINE, vehicle::StateSource::MOCK, 11};
    source.rtos = {BinaryState::OFF, StateCondition::OFFLINE, vehicle::StateSource::MOCK, 1};
    source.sensor = {BinaryState::OFF, StateCondition::OFFLINE, vehicle::StateSource::MOCK, 1};
    source.simulated_led = {BinaryState::ON, StateCondition::SIMULATED,
                            vehicle::StateSource::MOCK, 11};
    source.services[static_cast<std::size_t>(ServiceDomain::MEDIA)] =
        {ServiceHealth::ONLINE, vehicle::StateSource::MOCK, 1};
    source.services[static_cast<std::size_t>(ServiceDomain::VOICE)] =
        {ServiceHealth::DEGRADED, vehicle::StateSource::MOCK, 1};
    source.services[static_cast<std::size_t>(ServiceDomain::INFER)] =
        {ServiceHealth::OFFLINE, vehicle::StateSource::MOCK, 1};
    source.services[static_cast<std::size_t>(ServiceDomain::RTOS)] =
        {ServiceHealth::ONLINE, vehicle::StateSource::MOCK, 1};
    source.services[static_cast<std::size_t>(ServiceDomain::SYSTEM)] =
        {ServiceHealth::ONLINE, vehicle::StateSource::RUNTIME, 1};

    const auto mapped = mapVehicleState(source);
    CHECK(mapped.revision == 12);
    CHECK(mapped.backend_mode == "CORE / MOCK SERVICES");
    CHECK(mapped.camera_front.state == AvailabilityState::Online);
    CHECK(mapped.camera_rear.state == AvailabilityState::Unavailable);
    CHECK(mapped.recording_state == "Recording" && mapped.rtsp_state == "On");
    CHECK(mapped.media_state == "Playing" && mapped.voice_session == "Active");
    CHECK(mapped.rtos.state == AvailabilityState::Offline);
    CHECK(mapped.rtos_service.state == AvailabilityState::Online);
    CHECK(mapped.simulated_led_on);

    RevisionedStateProjector projector;
    UiState projected;
    auto revision10 = source;
    revision10.revision = 10;
    revision10.selected_camera.value = CameraSelection::FRONT;
    CHECK(projector.apply(revision10, projected));
    CHECK(projector.apply(source, projected));
    auto stale = source;
    stale.revision = 11;
    stale.selected_camera.value = CameraSelection::REAR;
    CHECK(!projector.apply(stale, projected));
    CHECK(projected.current_camera == "Front" && projector.lastRevision() == 12);
    CHECK(!projector.apply(source, projected));
    source.revision = 13;
    CHECK(projector.apply(source, projected) && projector.lastRevision() == 13);

    Fixture fixture;
    CHECK(fixture.core.start().ok());
    std::mutex results_mutex;
    std::vector<UiResult> results;
    fixture.backend.setResultCallback([&](UiResult result) {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(std::move(result));
    });
    CHECK(fixture.backend.start());
    CHECK(fixture.backend.currentState().camera_rear.state == AvailabilityState::Unavailable);
    CHECK(fixture.backend.currentState().rtos.state == AvailabilityState::Offline);
    CHECK(fixture.backend.currentState().simulated_controls.state == AvailabilityState::Simulated);

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::TIMEOUT);
    const auto controlled_start =
        fixture.backend.submit({UiCommand::RecordingStart, {}, false});
    CHECK(controlled_start.status == UiResultStatus::Accepted && !controlled_start.terminal);
    CHECK(waitUntil([&] {
        const auto state = fixture.backend.currentState();
        return state.recording_pending && state.recording.state == AvailabilityState::Starting &&
               fixture.client.get_snapshot().recording.value == RecordingState::STARTING &&
               fixture.media->invocation_count(CommandType::RECORDING_START) == 1;
    }));
    CHECK(fixture.media->complete_pending(controlled_start.request_id,
                                          protocol::Status::Ok()).ok());
    CHECK(waitUntil([&] {
        const auto state = fixture.backend.currentState();
        return !state.recording_pending && state.recording_state == "Recording";
    }));

    fixture.media->set_behavior(CommandType::RECORDING_STOP, MockBehavior::TIMEOUT);
    const auto controlled_stop = fixture.backend.submit({UiCommand::RecordingStop, {}, false});
    CHECK(controlled_stop.status == UiResultStatus::Accepted && !controlled_stop.terminal);
    CHECK(waitUntil([&] {
        const auto state = fixture.backend.currentState();
        return state.recording_pending && state.recording.state == AvailabilityState::Starting &&
               fixture.client.get_snapshot().recording.value == RecordingState::STOPPING &&
               fixture.media->invocation_count(CommandType::RECORDING_STOP) == 1;
    }));
    CHECK(fixture.media->complete_pending(controlled_stop.request_id,
                                          protocol::Status::Ok()).ok());
    CHECK(waitUntil([&] {
        const auto state = fixture.backend.currentState();
        return !state.recording_pending && state.recording_state == "Stopped";
    }));

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::TIMEOUT);
    const auto timeout_invocations =
        fixture.media->invocation_count(CommandType::RECORDING_START);
    const auto pending = fixture.backend.submit({UiCommand::RecordingStart, {}, false});
    CHECK(pending.status == UiResultStatus::Accepted && !pending.terminal);
    CHECK(waitUntil([&] {
        return fixture.media->invocation_count(CommandType::RECORDING_START) ==
                   timeout_invocations + 1 &&
               fixture.backend.currentState().recording_pending;
    }));
    fixture.clock->advance(2001);
    CHECK(fixture.core.poll_deadlines().ok());
    CHECK(waitUntil([&] {
        std::lock_guard<std::mutex> lock(results_mutex);
        return !results.empty() && results.back().request_id == pending.request_id;
    }));
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        CHECK(results.back().status == UiResultStatus::Timeout && results.back().terminal);
    }
    CHECK(fixture.backend.currentState().recording.state == AvailabilityState::Error);
    const auto timeout_revision = fixture.backend.currentState().revision;
    CHECK(fixture.media->complete_pending(pending.request_id, protocol::Status::Ok()).ok());
    CHECK(waitUntil([&] { return fixture.core.ignored_late_results() >= 1; }));
    CHECK(fixture.backend.currentState().revision == timeout_revision);

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::FAILURE);
    const auto failed = fixture.backend.submit({UiCommand::RecordingStart, {}, false});
    CHECK(failed.status == UiResultStatus::Accepted);
    CHECK(waitUntil([&] {
        std::lock_guard<std::mutex> lock(results_mutex);
        return !results.empty() && results.back().request_id == failed.request_id;
    }));
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        CHECK(results.back().status == UiResultStatus::Error);
    }

    const auto rear = fixture.backend.submit({UiCommand::SwitchCamera, "rear", false});
    CHECK(rear.status == UiResultStatus::Accepted);
    CHECK(waitUntil([&] {
        std::lock_guard<std::mutex> lock(results_mutex);
        return !results.empty() && results.back().request_id == rear.request_id;
    }));
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        CHECK(results.back().status == UiResultStatus::Unavailable);
    }
    CHECK(fixture.backend.currentState().current_camera == "Front");

    const auto led = fixture.backend.submit({UiCommand::LedSet, {}, true});
    CHECK(led.status == UiResultStatus::Accepted);
    CHECK(waitUntil([&] { return fixture.backend.currentState().simulated_led_on; }));
    CHECK(waitUntil([&] {
        std::lock_guard<std::mutex> lock(results_mutex);
        return !results.empty() && results.back().request_id == led.request_id;
    }));
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        CHECK(results.back().message.find("SIMULATED") != std::string::npos);
    }

    const auto submitAndAwait = [&](const UiRequest& request, MockServiceAdapter& adapter,
                                    CommandType type) {
        const auto before = adapter.invocation_count(type);
        const auto ack = fixture.backend.submit(request);
        if (ack.status != UiResultStatus::Accepted) return false;
        if (!waitUntil([&] { return adapter.invocation_count(type) == before + 1; })) return false;
        return waitUntil([&] {
            std::lock_guard<std::mutex> lock(results_mutex);
            return !results.empty() && results.back().request_id == ack.request_id;
        });
    };

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::SUCCESS);
    fixture.media->set_behavior(CommandType::RECORDING_STOP, MockBehavior::SUCCESS);
    CHECK(submitAndAwait({UiCommand::SwitchCamera, "front", false}, *fixture.media,
                         CommandType::CAMERA_SELECT));
    CHECK(submitAndAwait({UiCommand::Snapshot, {}, false}, *fixture.media,
                         CommandType::CAMERA_SNAPSHOT));
    CHECK(submitAndAwait({UiCommand::RecordingStart, {}, false}, *fixture.media,
                         CommandType::RECORDING_START));
    CHECK(submitAndAwait({UiCommand::RecordingStop, {}, false}, *fixture.media,
                         CommandType::RECORDING_STOP));
    CHECK(submitAndAwait({UiCommand::RtspStart, {}, false}, *fixture.media,
                         CommandType::RTSP_START));
    fixture.media->set_behavior(CommandType::RTSP_STOP, MockBehavior::FAILURE);
    CHECK(submitAndAwait({UiCommand::RtspStop, {}, false}, *fixture.media,
                         CommandType::RTSP_STOP));
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        CHECK(results.back().status == UiResultStatus::Error);
    }
    CHECK(fixture.backend.currentState().rtsp_state == "On");
    fixture.media->set_behavior(CommandType::RTSP_STOP, MockBehavior::SUCCESS);
    CHECK(submitAndAwait({UiCommand::RtspStop, {}, false}, *fixture.media,
                         CommandType::RTSP_STOP));
    CHECK(submitAndAwait({UiCommand::MediaPlay, {}, false}, *fixture.media,
                         CommandType::MEDIA_PLAY));
    CHECK(submitAndAwait({UiCommand::MediaPause, {}, false}, *fixture.media,
                         CommandType::MEDIA_PAUSE));
    CHECK(submitAndAwait({UiCommand::MediaPrevious, {}, false}, *fixture.media,
                         CommandType::MEDIA_PREVIOUS));
    CHECK(submitAndAwait({UiCommand::MediaNext, {}, false}, *fixture.media,
                         CommandType::MEDIA_NEXT));
    CHECK(submitAndAwait({UiCommand::MediaStop, {}, false}, *fixture.media,
                         CommandType::MEDIA_STOP));
    CHECK(submitAndAwait({UiCommand::VoiceSessionStart, {}, false}, *fixture.voice,
                         CommandType::VOICE_SESSION_START));
    CHECK(submitAndAwait({UiCommand::VoiceSessionCancel, {}, false}, *fixture.voice,
                         CommandType::VOICE_SESSION_CANCEL));
    CHECK(submitAndAwait({UiCommand::BuzzerSet, {}, true}, *fixture.rtos,
                         CommandType::SIM_BUZZER_SET));

    fixture.backend.stop();
    fixture.core.stop();

    {
        CoreIntegrationRuntime runtime(CoreDemoProfile::RtosOffline);
        CHECK(runtime.start());
        auto backend = runtime.makeUiBackend();
        CHECK(backend->start());
        const auto unavailable = backend->submit({UiCommand::BuzzerSet, {}, true});
        CHECK(unavailable.status == UiResultStatus::Unavailable && unavailable.terminal);
        CHECK(runtime.rtosAdapter()->invocation_count(CommandType::SIM_BUZZER_SET) == 0);
        CHECK(backend->currentState().rtos.state == AvailabilityState::Offline);
        CHECK(!backend->currentState().simulated_buzzer_on);
        backend->stop();
        runtime.stop();
    }

    for (int cycle = 0; cycle < 20; ++cycle) {
        CoreIntegrationRuntime runtime;
        CHECK(runtime.start());
        auto backend = runtime.makeUiBackend();
        CHECK(backend->start());
        const auto play = backend->submit({UiCommand::MediaPlay, {}, false});
        CHECK(play.status == UiResultStatus::Accepted);
        backend->stop();
        runtime.stop();
    }

    {
        cockpit::infer::VisionRuntimeSnapshot vision;
        vision.state = cockpit::infer::VisionRuntimeState::RUNNING;
        vision.backend.model_name = "MobileNetV1 RK3576";
        vision.metrics.vision_fps = 7.5;
        cockpit::infer::VisionResult result;
        result.camera_id = "front";
        result.stream_epoch = 3;
        result.frame_sequence = 99;
        result.classifications.push_back({42, "ImageNet class 42", 0.875F});
        vision.latest_result = result;
        UiState state;
        VisionUiBackend::applyVision(vision, state);
        CHECK(state.vision.source == cockpit::ui::StateSource::Runtime);
        CHECK(state.vision.state == AvailabilityState::Online);
        CHECK(state.vision_model == "MobileNetV1 RK3576");
        CHECK(state.current_camera == "front");
        CHECK(state.inference_rate == "7.50 FPS");
        CHECK(state.latest_inference.find("class 42") != std::string::npos);
        CHECK(state.latest_inference.find("e3 s99") != std::string::npos);
    }

    {
        cockpit::infer::SerialInferenceScheduler scheduler;
        auto fake_capture = std::make_unique<cockpit::media::FakeCameraCapture>();
        auto* capture_observer = fake_capture.get();
        auto fake_vision = std::make_unique<cockpit::infer::FakeVisionBackend>(scheduler);
        CoreIntegrationRuntimeOptions options;
        options.media_backend = MediaBackendKind::Cam0Real;
        options.camera.device = "fake-cam0";
        options.camera.camera_id = "front";
        options.camera.width = 8;
        options.camera.height = 4;
        options.camera.fps = 30;
        options.snapshot_directory = "/tmp/cockpit-vision-runtime-test";
        options.recording_directory = "/tmp/cockpit-vision-runtime-test";
        options.vision_backend = VisionBackendKind::RknnReal;
        CoreIntegrationRuntime runtime(std::move(options), std::move(fake_capture), {}, {}, {},
                                       std::move(fake_vision));
        CHECK(runtime.start());
        auto backend = runtime.makeUiBackend();
        CHECK(backend->start());
        CHECK(waitUntil([&] {
            return backend->currentState().latest_inference.find("class") !=
                   std::string::npos;
        }));
        const auto state = backend->currentState();
        CHECK(state.backend_mode == "CORE+VISION");
        CHECK(state.vision.source == cockpit::ui::StateSource::Runtime);
        CHECK(state.current_camera == "front");
        CHECK(capture_observer->open_count() == 1);
        CHECK(capture_observer->start_count() == 1);
        backend->stop();
        runtime.stop();
        CHECK(capture_observer->stop_count() == 1);
    }

    std::cout << "vehicle_core_backend_test: PASS\n";
    return 0;
}
