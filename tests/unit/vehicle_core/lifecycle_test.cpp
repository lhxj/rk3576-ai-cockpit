#include "test_support.hpp"

#include <iostream>
#include <mutex>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace cockpit::vehicle;
    using namespace cockpit::vehicle::test;
    Fixture fixture;
    CHECK(fixture.start_status.ok());
    std::mutex events_mutex;
    std::vector<std::uint64_t> revisions;
    CHECK(fixture.client.subscribe_state([&](const VehicleState& state) {
        std::lock_guard<std::mutex> lock(events_mutex);
        revisions.push_back(state.revision);
    }).ok());

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::TIMEOUT);
    auto start = fixture.command(100, CommandType::RECORDING_START);
    auto pending = fixture.client.send_command(start);
    CHECK(pending.accepted());
    CHECK(wait_until([&] { return fixture.client.get_snapshot().recording.value == RecordingState::STARTING; }));
    CHECK(!ready(pending.result));
    fixture.clock->advance(101);
    CHECK(fixture.core.poll_deadlines().ok());
    CHECK(pending.result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const auto timeout = pending.result.get();
    CHECK(timeout.status.code == protocol::StatusCode::TIMEOUT);
    CHECK(pending.ack.lifecycle_sequence < timeout.lifecycle_sequence);
    CHECK(wait_until([&] { return fixture.client.get_snapshot().recording.value == RecordingState::ERROR; }));
    const auto timeout_revision = fixture.client.get_snapshot().revision;
    CHECK(fixture.media->complete_pending(100, protocol::Status::Ok()).ok());
    CHECK(wait_until([&] { return fixture.core.ignored_late_results() >= 1; }));
    CHECK(fixture.client.get_snapshot().recording.value == RecordingState::ERROR);
    CHECK(fixture.client.get_snapshot().revision == timeout_revision);

    fixture.media->set_behavior(CommandType::RECORDING_STOP, MockBehavior::FAILURE);
    auto stop = fixture.client.send_command(fixture.command(101, CommandType::RECORDING_STOP));
    CHECK(stop.accepted());
    CHECK(stop.result.get().status.code == protocol::StatusCode::INTERNAL_ERROR);
    CHECK(fixture.client.get_snapshot().recording.value == RecordingState::ERROR);

    fixture.rtos->set_behavior(CommandType::SIM_LED_SET, MockBehavior::SUCCESS);
    auto led = fixture.client.send_command(fixture.command(102, CommandType::SIM_LED_SET,
                                                           {{"enabled", "true"}}));
    CHECK(led.accepted());
    const auto led_result = led.result.get();
    CHECK(led_result.status.ok() && led_result.simulated);
    const auto state = fixture.client.get_snapshot();
    CHECK(state.simulated_led.value == BinaryState::ON);
    CHECK(state.simulated_led.condition == StateCondition::SIMULATED);
    CHECK(state.rtos.value == BinaryState::OFF);

    {
        std::lock_guard<std::mutex> lock(events_mutex);
        CHECK(!revisions.empty());
        for (std::size_t i = 1; i < revisions.size(); ++i) CHECK(revisions[i] > revisions[i - 1]);
        CHECK(revisions.back() <= fixture.client.get_snapshot().revision);
    }

    fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::SUCCESS);
    auto success = fixture.client.send_command(fixture.command(103, CommandType::RECORDING_START));
    CHECK(success.accepted());
    const auto success_result = success.result.get();
    CHECK(success_result.status.ok());
    CHECK(success.ack.lifecycle_sequence < success_result.lifecycle_sequence);
    CHECK(fixture.client.get_snapshot().recording.value == RecordingState::RECORDING);
    CHECK(fixture.client.get_snapshot().services[static_cast<std::size_t>(ServiceDomain::MEDIA)].health ==
          ServiceHealth::ONLINE);
    const auto before_health = fixture.client.get_snapshot().revision;
    CHECK(fixture.core.set_service_health(ServiceDomain::MEDIA, ServiceHealth::OFFLINE).ok());
    CHECK(wait_until([&] {
        return fixture.client.get_snapshot().services[static_cast<std::size_t>(ServiceDomain::MEDIA)].health ==
               ServiceHealth::OFFLINE;
    }));
    CHECK(fixture.client.get_snapshot().revision > before_health);
    auto offline = fixture.client.send_command(fixture.command(104, CommandType::RECORDING_STOP));
    CHECK(!offline.ack_emitted && offline.status.code == protocol::StatusCode::UNAVAILABLE);
    return 0;
}
