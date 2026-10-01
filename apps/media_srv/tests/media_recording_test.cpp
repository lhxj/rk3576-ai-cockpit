#include "cockpit/media/fake_media_recorder.hpp"
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
namespace media = cockpit::media;
namespace vehicle = cockpit::vehicle;
namespace protocol = cockpit::protocol;

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = 2s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

media::MediaServiceConfig media_config(const std::filesystem::path& output,
                                       std::size_t queue_capacity = 12) {
    media::MediaServiceConfig config;
    config.capture.device = "fake-cam0";
    config.capture.camera_id = "front";
    config.capture.width = 16;
    config.capture.height = 8;
    config.capture.pixel_format = "NV12";
    config.capture.fps = 30;
    config.capture.buffer_count = 4;
    config.snapshot_directory = output.string();
    config.recording_directory = output.string();
    config.recorder.queue_capacity = queue_capacity;
    config.recorder.first_packet_timeout = 1s;
    return config;
}

vehicle::VehicleCommand command(vehicle::VehicleCore& core, vehicle::IClock& clock,
                                protocol::RequestId request_id,
                                vehicle::CommandType type,
                                protocol::Deadline deadline_delta = 2000) {
    vehicle::VehicleCommand value;
    value.request_id = request_id;
    value.boot_epoch = core.boot_epoch();
    value.deadline_ms = clock.now_ms() + deadline_delta;
    value.source = vehicle::CommandSource::TEST;
    value.command_type = type;
    return value;
}

struct Fixture {
    Fixture(const std::filesystem::path& output,
            media::FakeCameraCaptureOptions camera_options = {},
            media::FakeMediaRecorderOptions recorder_options = {},
            std::size_t queue_capacity = 12,
            std::shared_ptr<vehicle::IClock> supplied_clock =
                std::make_shared<vehicle::SystemClock>())
        : clock(std::move(supplied_clock)), camera(std::make_unique<media::FakeCameraCapture>(
                                               std::move(camera_options))),
          camera_ptr(camera.get()),
          recorder(std::make_unique<media::FakeMediaRecorder>(std::move(recorder_options))),
          recorder_ptr(recorder.get()),
          service(std::make_shared<media::MediaService>(
              media_config(output, queue_capacity), std::move(camera),
              std::make_shared<media::PreviewMailbox>(), std::move(recorder))),
          adapter(std::make_shared<media::RealMediaServiceAdapter>(service)),
          voice(std::make_shared<vehicle::MockVoiceAdapter>()),
          rtos(std::make_shared<vehicle::MockRtosAdapter>()),
          system(std::make_shared<vehicle::MockSystemAdapter>()),
          registry(std::make_shared<vehicle::ServiceRegistry>()),
          core({20261002, 32, 32, 128, 1ms},
               {adapter, voice, rtos, system}, registry, clock),
          client(core) {
        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        adapter->set_runtime_state_callback(
            [this](vehicle::CommandType type, vehicle::AdapterResult result) {
                (void)core.report_runtime_result(type, std::move(result));
            });
    }

    ~Fixture() {
        core.stop();
        service->stop();
    }

    void start() {
        if (!service->start().ok() || !core.start().ok())
            throw std::runtime_error("fixture start");
    }

    std::shared_ptr<vehicle::IClock> clock;
    std::unique_ptr<media::FakeCameraCapture> camera;
    media::FakeCameraCapture* camera_ptr;
    std::unique_ptr<media::FakeMediaRecorder> recorder;
    media::FakeMediaRecorder* recorder_ptr;
    std::shared_ptr<media::MediaService> service;
    std::shared_ptr<media::RealMediaServiceAdapter> adapter;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos;
    std::shared_ptr<vehicle::MockSystemAdapter> system;
    std::shared_ptr<vehicle::ServiceRegistry> registry;
    vehicle::VehicleCore core;
    vehicle::InProcessVehicleCoreClient client;
};

vehicle::CommandResult require_result(vehicle::CommandSubmission& submission) {
    if (submission.result.wait_for(3s) != std::future_status::ready)
        throw std::runtime_error("command result timeout");
    return submission.result.get();
}

}  // namespace

int main() {
    const auto output = std::filesystem::temp_directory_path() /
                        ("cockpit-recording-test-" + std::to_string(
                            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(output);

    media::FakeMediaRecorderOptions delayed;
    delayed.encode_delay = 40ms;
    Fixture shared(output, {}, delayed);
    shared.start();

    auto preview_start = shared.client.send_command(command(
        shared.core, *shared.clock, 1, vehicle::CommandType::CAMERA_PREVIEW_START));
    CHECK(preview_start.accepted());
    CHECK(require_result(preview_start).status.ok());
    CHECK(shared.camera_ptr->start_count() == 1);

    auto record_start = shared.client.send_command(command(
        shared.core, *shared.clock, 2, vehicle::CommandType::RECORDING_START));
    CHECK(record_start.accepted() && record_start.ack.lifecycle_sequence > 0);
    CHECK(wait_until([&] {
        return shared.client.get_snapshot().recording.value ==
               vehicle::RecordingState::STARTING;
    }));
    CHECK(record_start.result.wait_for(5ms) == std::future_status::timeout);
    CHECK(require_result(record_start).status.ok());
    CHECK(shared.client.get_snapshot().recording.value ==
          vehicle::RecordingState::RECORDING);
    CHECK(shared.client.get_snapshot().recording.source ==
          vehicle::StateSource::RUNTIME);
    CHECK(shared.camera_ptr->open_count() == 1 &&
          shared.camera_ptr->start_count() == 1);

    const auto start_requests = shared.service->service_stats().recording_start_requests;
    auto duplicate_start = shared.client.send_command(command(
        shared.core, *shared.clock, 3, vehicle::CommandType::RECORDING_START));
    CHECK(duplicate_start.accepted());
    CHECK(require_result(duplicate_start).status.ok());
    CHECK(shared.service->service_stats().recording_start_requests == start_requests);

    auto record_stop = shared.client.send_command(command(
        shared.core, *shared.clock, 4, vehicle::CommandType::RECORDING_STOP));
    CHECK(record_stop.accepted());
    CHECK(wait_until([&] {
        return shared.client.get_snapshot().recording.value ==
               vehicle::RecordingState::STOPPING;
    }));
    CHECK(require_result(record_stop).status.ok());
    CHECK(shared.client.get_snapshot().recording.value ==
          vehicle::RecordingState::STOPPED);
    CHECK(shared.service->preview_active() && shared.service->streaming());
    CHECK(shared.camera_ptr->stop_count() == 0);
    CHECK(shared.service->recorder_stats().file_closed);

    const auto stop_requests = shared.service->service_stats().recording_stop_requests;
    auto duplicate_stop = shared.client.send_command(command(
        shared.core, *shared.clock, 5, vehicle::CommandType::RECORDING_STOP));
    CHECK(duplicate_stop.accepted());
    CHECK(require_result(duplicate_stop).status.ok());
    CHECK(shared.service->service_stats().recording_stop_requests == stop_requests);

    auto preview_stop = shared.client.send_command(command(
        shared.core, *shared.clock, 6, vehicle::CommandType::CAMERA_PREVIEW_STOP));
    CHECK(preview_stop.accepted() && require_result(preview_stop).status.ok());
    CHECK(!shared.service->streaming() && shared.camera_ptr->stop_count() == 1);

    auto recording_only = shared.client.send_command(command(
        shared.core, *shared.clock, 7, vehicle::CommandType::RECORDING_START));
    CHECK(recording_only.accepted() && require_result(recording_only).status.ok());
    CHECK(shared.service->streaming() && shared.service->recording_active());
    CHECK(!shared.service->preview_active());
    CHECK(shared.client.get_snapshot().preview.value ==
          vehicle::PreviewState::STOPPED);
    CHECK(shared.camera_ptr->start_count() == 2);
    auto recording_only_stop = shared.client.send_command(command(
        shared.core, *shared.clock, 8, vehicle::CommandType::RECORDING_STOP));
    CHECK(recording_only_stop.accepted() &&
          require_result(recording_only_stop).status.ok());
    CHECK(!shared.service->streaming());

    media::FakeCameraCaptureOptions fast_camera;
    fast_camera.frame_interval = 1ms;
    media::FakeMediaRecorderOptions slow_recorder;
    slow_recorder.encode_delay = 100ms;
    Fixture overflow(output / "overflow", fast_camera, slow_recorder, 1);
    overflow.start();
    auto overflow_start = overflow.client.send_command(command(
        overflow.core, *overflow.clock, 20, vehicle::CommandType::RECORDING_START));
    CHECK(overflow_start.accepted());
    const auto overflow_result = require_result(overflow_start);
    CHECK(overflow_result.status.code == protocol::StatusCode::INTERNAL_ERROR);
    CHECK(overflow_result.status.detail == "RECORDING_BACKPRESSURE");
    CHECK(wait_until([&] {
        return overflow.client.get_snapshot().recording.value ==
               vehicle::RecordingState::ERROR;
    }));
    CHECK(overflow.service->recorder_stats().overflow_count == 1);
    overflow.core.stop();
    overflow.service->stop();
    CHECK(!overflow.service->streaming());

    media::FakeMediaRecorderOptions start_failure;
    start_failure.fail_start = true;
    Fixture failed_start(output / "start-failure", {}, start_failure);
    failed_start.start();
    auto failed_start_command = failed_start.client.send_command(command(
        failed_start.core, *failed_start.clock, 21,
        vehicle::CommandType::RECORDING_START));
    CHECK(failed_start_command.accepted());
    CHECK(require_result(failed_start_command).status.code ==
          protocol::StatusCode::UNAVAILABLE);
    CHECK(failed_start.client.get_snapshot().recording.value ==
          vehicle::RecordingState::ERROR);
    CHECK(!failed_start.service->streaming());

    media::FakeMediaRecorderOptions asynchronous_failure;
    asynchronous_failure.fail_after_packets = 3;
    Fixture failed_encode(output / "encode-failure", {}, asynchronous_failure);
    failed_encode.start();
    auto failed_encode_start = failed_encode.client.send_command(command(
        failed_encode.core, *failed_encode.clock, 22,
        vehicle::CommandType::RECORDING_START));
    CHECK(failed_encode_start.accepted() &&
          require_result(failed_encode_start).status.ok());
    CHECK(wait_until([&] {
        return failed_encode.client.get_snapshot().recording.value ==
               vehicle::RecordingState::ERROR;
    }));
    CHECK(failed_encode.service->recorder_stats().encoder_errors == 1);
    CHECK(failed_encode.service->recorder_stats().file_closed);
    auto stop_after_failure = failed_encode.client.send_command(command(
        failed_encode.core, *failed_encode.clock, 23,
        vehicle::CommandType::RECORDING_STOP));
    CHECK(stop_after_failure.accepted() &&
          require_result(stop_after_failure).status.ok());
    auto restart_after_failure = failed_encode.client.send_command(command(
        failed_encode.core, *failed_encode.clock, 24,
        vehicle::CommandType::RECORDING_START));
    CHECK(restart_after_failure.accepted() &&
          require_result(restart_after_failure).status.ok());
    CHECK(wait_until([&] {
        return failed_encode.client.get_snapshot().recording.value ==
               vehicle::RecordingState::ERROR;
    }));
    failed_encode.core.stop();
    failed_encode.service->stop();
    CHECK(failed_encode.service->recorder_stats().file_closed);
    CHECK(!failed_encode.service->streaming());

    auto fake_clock = std::make_shared<vehicle::FakeClock>(1000);
    media::FakeMediaRecorderOptions late_recorder;
    late_recorder.encode_delay = 100ms;
    Fixture timeout(output / "timeout", {}, late_recorder, 12, fake_clock);
    timeout.start();
    auto timed = timeout.client.send_command(command(
        timeout.core, *timeout.clock, 30, vehicle::CommandType::RECORDING_START, 5));
    CHECK(timed.accepted());
    CHECK(wait_until([&] {
        return timeout.client.get_snapshot().recording.value ==
               vehicle::RecordingState::STARTING;
    }));
    fake_clock->advance(6);
    CHECK(timeout.core.poll_deadlines().ok());
    CHECK(timed.result.wait_for(1s) == std::future_status::ready);
    CHECK(timed.result.get().status.code == protocol::StatusCode::TIMEOUT);
    const auto timeout_revision = timeout.client.get_snapshot().revision;
    CHECK(wait_until([&] { return timeout.core.ignored_late_results() >= 1; }));
    CHECK(timeout.client.get_snapshot().revision == timeout_revision);
    CHECK(timeout.client.get_snapshot().recording.value ==
          vehicle::RecordingState::ERROR);
    timeout.core.stop();
    timeout.service->stop();
    CHECK(timeout.service->recorder_stats().file_closed);
    CHECK(!timeout.service->streaming());

    std::error_code ignored;
    std::filesystem::remove_all(output, ignored);
    std::cout << "media_recording_test: PASS\n";
    return 0;
}
