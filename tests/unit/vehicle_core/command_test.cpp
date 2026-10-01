#include "test_support.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace cockpit::vehicle;
    using namespace cockpit::vehicle::test;
    Fixture fixture;
    CHECK(fixture.start_status.ok());

    auto command = fixture.command(1, CommandType::CAMERA_SELECT, {{"camera", "front"}});
    CHECK(validate_command_shape(command, 77, fixture.clock->now_ms()).ok());
    auto bad = command;
    bad.request_id = 2;
    bad.command_type = static_cast<CommandType>(999);
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::INVALID_ARGUMENT);
    bad = command;
    bad.request_id = 3;
    bad.parameters = {{"camera", "side"}};
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::INVALID_ARGUMENT);
    bad = command;
    bad.request_id = 4;
    bad.deadline_ms = 999;
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::EXPIRED);
    bad = command;
    bad.request_id = 5;
    bad.boot_epoch = 76;
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::STALE_EPOCH);
    bad = command;
    bad.request_id = 6;
    bad.protocol_version = 2;
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::UNSUPPORTED_VERSION);
    bad = command;
    bad.request_id = 7;
    bad.message_type = protocol::MessageType::RESULT;
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::MALFORMED);
    bad = command;
    bad.request_id = 8;
    bad.source = CommandSource::REMOTE;
    CHECK(fixture.client.send_command(bad).status.code == protocol::StatusCode::INVALID_STATE);

    auto rear = fixture.command(9, CommandType::CAMERA_SELECT, {{"camera", "rear"}});
    auto rear_submission = fixture.client.send_command(rear);
    CHECK(rear_submission.accepted());
    CHECK(rear_submission.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(rear_submission.result.get().status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(fixture.client.get_snapshot().selected_camera.value == CameraSelection::FRONT);
    CHECK(fixture.media->invocation_count(CommandType::CAMERA_SELECT) == 0);

    auto start = fixture.command(10, CommandType::RECORDING_START);
    auto first = fixture.client.send_command(start);
    CHECK(first.accepted());
    CHECK(first.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(first.result.get().status.ok());
    auto replay = fixture.client.send_command(start);
    CHECK(replay.replayed());
    CHECK(replay.result.get().status.ok());
    CHECK(fixture.media->invocation_count(CommandType::RECORDING_START) == 1);
    auto collision = start;
    collision.command_type = CommandType::RECORDING_STOP;
    CHECK(fixture.client.send_command(collision).status.code == protocol::StatusCode::DUPLICATE_REQUEST);

    auto idempotent = fixture.command(11, CommandType::RECORDING_START);
    auto idempotent_result = fixture.client.send_command(idempotent);
    CHECK(idempotent_result.accepted());
    CHECK(idempotent_result.result.get().status.ok());
    CHECK(fixture.media->invocation_count(CommandType::RECORDING_START) == 1);

    protocol::Message message;
    auto query = fixture.command(12, CommandType::QUERY_STATE);
    CHECK(encode_command_message(query, message).ok());
    auto decoded = decode_command_message(message);
    CHECK(decoded.status.ok() && decoded.command.request_id == 12);
    LoopbackVehicleCoreClient loopback(fixture.core, 4);
    CHECK(loopback.start().ok());
    auto loopback_result = loopback.send_command(query);
    CHECK(loopback_result.accepted());
    CHECK(loopback_result.result.get().status.ok());
    loopback.stop();

    const auto snapshot = fixture.client.get_snapshot();
    CHECK(snapshot.rear_camera.source == StateSource::MOCK);
    CHECK(snapshot.rear_camera.value == CameraAvailability::UNAVAILABLE);
    CHECK(snapshot.rtos.value == BinaryState::OFF);
    CHECK(snapshot.sensor.value == BinaryState::OFF);
    CHECK(snapshot.simulated_led.condition == StateCondition::SIMULATED);

    Fixture offline(16, false);
    offline.registry->set(ServiceDomain::MEDIA, ServiceHealth::OFFLINE, StateSource::MOCK);
    CHECK(offline.core.start().ok());
    auto unavailable = offline.client.send_command(offline.command(20, CommandType::RECORDING_START));
    CHECK(unavailable.status.code == protocol::StatusCode::UNAVAILABLE);
    CHECK(!unavailable.ack_emitted && unavailable.disposition == SubmissionDisposition::REJECTED);
    CHECK(offline.media->invocation_count(CommandType::RECORDING_START) == 0);
    offline.registry->set(ServiceDomain::RTOS, ServiceHealth::OFFLINE, StateSource::MOCK);
    CHECK(offline.client.send_command(offline.command(21, CommandType::SIM_BUZZER_SET,
          {{"enabled", "true"}})).status.code == protocol::StatusCode::UNAVAILABLE);
    return 0;
}
