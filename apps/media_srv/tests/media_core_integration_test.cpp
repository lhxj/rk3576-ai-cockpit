#include "cockpit/media/fake_camera_capture.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#define CHECK(expression)                                                                    \
    do {                                                                                     \
        if (!(expression)) {                                                                 \
            std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #expression << '\n';   \
            return 1;                                                                        \
        }                                                                                    \
    } while (false)

namespace {
using namespace std::chrono_literals;
using namespace cockpit;

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = 1s) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::yield();
    }
    return predicate();
}

media::MediaServiceConfig media_config(const std::filesystem::path& output) {
    media::MediaServiceConfig config;
    config.capture.device = "fake-cam0";
    config.capture.camera_id = "front";
    config.capture.width = 8;
    config.capture.height = 4;
    config.capture.pixel_format = "NV12";
    config.capture.fps = 30;
    config.capture.buffer_count = 4;
    config.snapshot_directory = output.string();
    config.snapshot_wait = 500ms;
    return config;
}

vehicle::VehicleCommand command(vehicle::VehicleCore& core, vehicle::FakeClock& clock,
                                protocol::RequestId id, vehicle::CommandType type,
                                vehicle::CommandParameters parameters = {}) {
    vehicle::VehicleCommand value;
    value.request_id = id;
    value.boot_epoch = core.boot_epoch();
    value.deadline_ms = clock.now_ms() + 100;
    value.source = vehicle::CommandSource::TEST;
    value.command_type = type;
    value.parameters = std::move(parameters);
    return value;
}

struct CoreFixture {
    CoreFixture(std::unique_ptr<media::FakeCameraCapture> capture,
                const std::filesystem::path& output)
        : service(std::make_shared<media::MediaService>(media_config(output), std::move(capture))),
          media_adapter(std::make_shared<media::RealMediaServiceAdapter>(service)),
          voice(std::make_shared<vehicle::MockVoiceAdapter>()),
          rtos(std::make_shared<vehicle::MockRtosAdapter>()),
          system(std::make_shared<vehicle::MockSystemAdapter>()),
          registry(std::make_shared<vehicle::ServiceRegistry>()),
          clock(std::make_shared<vehicle::FakeClock>(1000)),
          core({99, 16, 16, 64, 1ms}, {media_adapter, voice, rtos, system}, registry, clock),
          client(core) {
        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
    }
    ~CoreFixture() {
        core.stop();
        service->stop();
    }
    bool start() { return service->start().ok() && core.start().ok(); }

    std::shared_ptr<media::MediaService> service;
    std::shared_ptr<media::RealMediaServiceAdapter> media_adapter;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos;
    std::shared_ptr<vehicle::MockSystemAdapter> system;
    std::shared_ptr<vehicle::ServiceRegistry> registry;
    std::shared_ptr<vehicle::FakeClock> clock;
    vehicle::VehicleCore core;
    vehicle::InProcessVehicleCoreClient client;
};

vehicle::CommandResult successful(vehicle::CommandSubmission submission) {
    if (!submission.accepted() || submission.result.wait_for(1s) != std::future_status::ready)
        return {};
    return submission.result.get();
}

}  // namespace

int main() {
    const auto output = std::filesystem::temp_directory_path() /
                        ("cockpit-media-core-" + std::to_string(
                            std::chrono::steady_clock::now().time_since_epoch().count()));

    CoreFixture fixture(std::make_unique<media::FakeCameraCapture>(), output);
    CHECK(fixture.start());
    auto state = fixture.client.get_snapshot();
    CHECK(state.front_camera.source == vehicle::StateSource::RUNTIME);
    CHECK(state.preview.value == vehicle::PreviewState::STOPPED);
    CHECK(state.recording.condition == vehicle::StateCondition::OFFLINE);
    CHECK(state.rtsp.condition == vehicle::StateCondition::OFFLINE);

    auto select_submission = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 1, vehicle::CommandType::CAMERA_SELECT,
        {{"camera", "front"}}));
    CHECK(select_submission.accepted() && select_submission.ack_emitted);
    const auto select = successful(std::move(select_submission));
    CHECK(select.status.ok());
    state = fixture.client.get_snapshot();
    CHECK(state.selected_camera.value == vehicle::CameraSelection::FRONT);
    CHECK(state.selected_camera.source == vehicle::StateSource::RUNTIME);

    const auto before_start_revision = state.revision;
    auto start_submission = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 2, vehicle::CommandType::CAMERA_PREVIEW_START));
    CHECK(start_submission.accepted());
    CHECK(start_submission.ack.lifecycle_sequence != 0);
    const auto start = successful(std::move(start_submission));
    CHECK(start.status.ok() && start.lifecycle_sequence != 0);
    state = fixture.client.get_snapshot();
    CHECK(state.revision > before_start_revision);
    CHECK(state.preview.value == vehicle::PreviewState::STREAMING);
    CHECK(state.preview.source == vehicle::StateSource::RUNTIME);

    CHECK(wait_until([&] { return fixture.service->capture_stats().frames >= 2; }));
    const auto epoch = fixture.service->capture_stats().stream_epoch;
    CHECK(epoch == 1);
    const auto repeated_start = successful(fixture.client.send_command(command(
        fixture.core, *fixture.clock, 3, vehicle::CommandType::CAMERA_PREVIEW_START)));
    CHECK(repeated_start.status.ok());
    CHECK(fixture.service->capture_stats().stream_epoch == epoch);

    const auto snapshot = successful(fixture.client.send_command(command(
        fixture.core, *fixture.clock, 4, vehicle::CommandType::CAMERA_SNAPSHOT)));
    CHECK(snapshot.status.ok());
    CHECK(fixture.service->service_stats().snapshots_written == 1);

    const auto recording = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 5, vehicle::CommandType::RECORDING_START));
    CHECK(recording.status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(!recording.ack_emitted);
    const auto rtsp = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 6, vehicle::CommandType::RTSP_START));
    CHECK(rtsp.status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(!rtsp.ack_emitted);

    auto rear_submission = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 7, vehicle::CommandType::CAMERA_SELECT,
        {{"camera", "rear"}}));
    CHECK(rear_submission.accepted());
    const auto rear = successful(std::move(rear_submission));
    CHECK(rear.status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(fixture.client.get_snapshot().selected_camera.value == vehicle::CameraSelection::FRONT);

    auto stop_submission = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 8, vehicle::CommandType::CAMERA_PREVIEW_STOP));
    CHECK(stop_submission.accepted());
    const auto stop = successful(std::move(stop_submission));
    CHECK(stop.status.ok());
    state = fixture.client.get_snapshot();
    CHECK(state.preview.value == vehicle::PreviewState::STOPPED);
    CHECK(state.preview.source == vehicle::StateSource::RUNTIME);
    const auto repeated_stop = successful(fixture.client.send_command(command(
        fixture.core, *fixture.clock, 9, vehicle::CommandType::CAMERA_PREVIEW_STOP)));
    CHECK(repeated_stop.status.ok());
    const auto stopped_snapshot = successful(fixture.client.send_command(command(
        fixture.core, *fixture.clock, 10, vehicle::CommandType::CAMERA_SNAPSHOT)));
    CHECK(stopped_snapshot.status.code == protocol::StatusCode::INVALID_STATE);

    auto held_capture = std::make_unique<media::FakeCameraCapture>(
        media::FakeCameraCaptureOptions{false, false, false, true, 10ms});
    auto* held_capture_pointer = held_capture.get();
    CoreFixture timeout_fixture(std::move(held_capture), output / "timeout");
    CHECK(timeout_fixture.start());
    auto timeout_submission = timeout_fixture.client.send_command(command(
        timeout_fixture.core, *timeout_fixture.clock, 100,
        vehicle::CommandType::CAMERA_PREVIEW_START));
    CHECK(timeout_submission.accepted());
    CHECK(held_capture_pointer->wait_until_start_entered(1s));
    timeout_fixture.clock->advance(101);
    CHECK(timeout_fixture.core.poll_deadlines().ok());
    CHECK(timeout_submission.result.wait_for(1s) == std::future_status::ready);
    CHECK(timeout_submission.result.get().status.code == protocol::StatusCode::TIMEOUT);
    const auto timeout_state = timeout_fixture.client.get_snapshot();
    CHECK(timeout_state.preview.value == vehicle::PreviewState::ERROR);
    CHECK(timeout_state.preview.source == vehicle::StateSource::RUNTIME);
    const auto timeout_revision = timeout_state.revision;
    held_capture_pointer->release_start();
    CHECK(wait_until([&] { return timeout_fixture.core.ignored_late_results() >= 1; }));
    CHECK(timeout_fixture.client.get_snapshot().revision == timeout_revision);
    CHECK(timeout_fixture.client.get_snapshot().preview.value == vehicle::PreviewState::ERROR);

    std::error_code ignored;
    std::filesystem::remove_all(output, ignored);
    std::cout << "media_core_integration_test: PASS\n";
    return 0;
}
