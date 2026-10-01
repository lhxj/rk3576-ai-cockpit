#include "../unit/vehicle_core/test_support.hpp"

#include "cockpit/vehicle/voice_command_sink.hpp"
#include "cockpit/voice/intent_dispatcher.hpp"

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <vector>

#define REQUIRE(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return false; } } while (false)

namespace {
using namespace cockpit;
using namespace cockpit::vehicle;
using namespace cockpit::vehicle::test;

class CapturingClient final : public IVehicleCoreClient {
public:
    explicit CapturingClient(IVehicleCoreClient& inner) : inner_(inner) {}
    CommandSubmission send_command(const VehicleCommand& command) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_ = command;
            ++count_;
        }
        return inner_.send_command(command);
    }
    VehicleState get_snapshot() const override { return inner_.get_snapshot(); }
    protocol::BootEpoch boot_epoch() const override { return inner_.boot_epoch(); }
    protocol::Status subscribe_state(StateCallback callback) override {
        return inner_.subscribe_state(std::move(callback));
    }
    std::optional<VehicleCommand> last() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_;
    }
    int count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }
private:
    IVehicleCoreClient& inner_;
    mutable std::mutex mutex_;
    std::optional<VehicleCommand> last_;
    int count_{0};
};

class Reports {
public:
    void add(const voice::IntentDispatchReport& report) {
        std::lock_guard<std::mutex> lock(mutex_);
        reports_.push_back(report);
        ready_.notify_all();
    }
    bool wait(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, std::chrono::seconds(1), [&] { return reports_.size() >= count; });
    }
    voice::IntentDispatchReport at(std::size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reports_.at(index);
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<voice::IntentDispatchReport> reports_;
};

struct Harness {
    explicit Harness(std::size_t queue_capacity = 8)
        : fixture(), controller(77), client(fixture.client), sink(client, fixture.clock, 77),
          dispatcher(router, controller, sink, [this] { return fixture.clock->now_ms(); },
                     [this](const voice::IntentDispatchReport& report) { reports.add(report); },
                     queue_capacity, 16) {}

    bool start() { return fixture.start_status.ok() && dispatcher.start().ok(); }
    voice::SessionToken session(protocol::RequestId id, protocol::Deadline deadline = 1100) {
        const auto token = controller.start(id);
        if (!controller.transition(token, voice::VoiceSessionState::Recognizing).ok() ||
            !sink.activate_session(token, deadline).ok()) return {};
        return token;
    }
    voice::AsrEvent event(voice::SessionToken token, const std::string& text,
                          std::uint64_t sequence = 1,
                          voice::AsrEventType type = voice::AsrEventType::FINAL) const {
        return {type, token, sequence, text, protocol::Status::Ok()};
    }
    protocol::Status deliver(const voice::AsrEvent& event, protocol::Deadline deadline = 1100) {
        protocol::Status queued;
        auto kind = event.type == voice::AsrEventType::FINAL ? protocol::MessageType::ASR_FINAL
                                                              : protocol::MessageType::ASR_PARTIAL;
        const auto delivered = controller.deliver_event(event.token, kind, [&] {
            queued = dispatcher.enqueue_event(event, deadline);
        });
        return delivered.ok() ? queued : delivered;
    }

    Fixture fixture;
    voice::DeterministicIntentRouter router;
    voice::VoiceSessionController controller;
    CapturingClient client;
    VehicleCommandSinkAdapter sink;
    Reports reports;
    voice::VoiceIntentDispatcher dispatcher;
};

bool test_normal_ack_result_state() {
    Harness h;
    REQUIRE(h.start());
    h.fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::TIMEOUT);
    const auto token = h.session(201);
    REQUIRE(token.session_id != 0);
    REQUIRE(h.deliver(h.event(token, "开始录像")).ok());
    REQUIRE(h.reports.wait(1));
    const auto report = h.reports.at(0);
    REQUIRE(report.outcome == voice::IntentOutcome::MATCH && report.submitted);
    const auto submission = h.sink.last_submission();
    REQUIRE(submission && submission->accepted() && submission->ack_emitted);
    REQUIRE(submission->ack.request_id == token.request_id);
    REQUIRE(submission->ack.session_id == token.session_id);
    REQUIRE(submission->ack.boot_epoch == token.boot_epoch);
    REQUIRE(wait_until([&] { return h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1; }));
    REQUIRE(!ready(submission->result));
    REQUIRE(h.client.get_snapshot().recording.value == RecordingState::STARTING);
    const auto command = h.client.last();
    REQUIRE(command && command->command_type == CommandType::RECORDING_START);
    REQUIRE(command->source == CommandSource::VOICE && command->parameters.empty());
    REQUIRE(command->request_id == token.request_id && command->session_id == token.session_id);
    REQUIRE(command->boot_epoch == token.boot_epoch && command->deadline_ms == 1100);
    REQUIRE(command->voice_generation == token.generation && command->asr_sequence == 1);
    protocol::Message encoded;
    REQUIRE(encode_command_message(*command, encoded).ok());
    const auto decoded = decode_command_message(encoded);
    REQUIRE(decoded.status.ok());
    REQUIRE(decoded.command.voice_generation == token.generation);
    REQUIRE(decoded.command.asr_sequence == 1);
    REQUIRE(decoded.command.request_id == token.request_id &&
            decoded.command.session_id == token.session_id &&
            decoded.command.boot_epoch == token.boot_epoch &&
            decoded.command.deadline_ms == command->deadline_ms);
    REQUIRE(h.fixture.media->complete_pending(token.request_id, protocol::Status::Ok()).ok());
    REQUIRE(submission->result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const auto result = submission->result.get();
    REQUIRE(result.status.ok() && !result.simulated);
    REQUIRE(submission->ack.lifecycle_sequence < result.lifecycle_sequence);
    REQUIRE(h.client.get_snapshot().recording.value == RecordingState::RECORDING);
    h.dispatcher.stop();
    REQUIRE(h.dispatcher.stats().submitted == 1);
    return true;
}

bool test_rejections_and_partial() {
    Harness h;
    REQUIRE(h.start());
    const auto token = h.session(202);
    REQUIRE(h.deliver(h.event(token, "不要开始录像")).ok());
    REQUIRE(h.deliver(h.event(token, "今天天气怎么样", 2)).ok());
    REQUIRE(h.deliver(h.event(token, "开始录像", 3, voice::AsrEventType::PARTIAL)).ok());
    REQUIRE(h.dispatcher.enqueue_event(h.event(token, std::string(513, 'x'), 4), 1100).code ==
            protocol::StatusCode::INVALID_ARGUMENT);
    REQUIRE(h.reports.wait(2));
    REQUIRE(h.reports.at(0).outcome == voice::IntentOutcome::REJECTED_NEGATED);
    REQUIRE(h.reports.at(1).outcome == voice::IntentOutcome::NO_MATCH);
    REQUIRE(h.client.count() == 0);
    REQUIRE(h.dispatcher.stats().enqueued == 2);
    h.dispatcher.stop();
    return true;
}

bool test_duplicate_and_core_replay() {
    Harness h;
    REQUIRE(h.start());
    const auto token = h.session(203);
    const auto final = h.event(token, "开始录像", 5);
    protocol::Status first;
    protocol::Status second;
    REQUIRE(h.controller.deliver_event(token, protocol::MessageType::ASR_FINAL, [&] {
        first = h.dispatcher.enqueue_event(final, 1100);
        second = h.dispatcher.enqueue_event(final, 1100);
        // The worker cannot route through the controller while this callback owns its lock.
        if (h.client.count() != 0) first = {protocol::StatusCode::INTERNAL_ERROR, "reentrant Core call"};
    }).ok());
    REQUIRE(first.ok() && second.ok());
    REQUIRE(h.reports.wait(2));
    REQUIRE(h.dispatcher.stats().duplicates == 1 && h.dispatcher.stats().submitted == 1);
    REQUIRE(h.reports.at(1).duplicate);
    REQUIRE(h.client.count() == 1);
    const auto original = h.sink.last_submission();
    REQUIRE(original && original->accepted());
    REQUIRE(original->result.get().status.ok());
    REQUIRE(h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1);
    const auto captured = h.client.last();
    REQUIRE(captured);
    const auto replay = h.fixture.client.send_command(*captured);
    REQUIRE(replay.replayed() && replay.result.get().status.ok());
    REQUIRE(h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1);
    REQUIRE(h.dispatcher.stats().cache_peak <= 16);
    h.dispatcher.stop();
    return true;
}

bool test_cancel_stale_deadline() {
    Harness h;
    REQUIRE(h.start());
    const auto old = h.session(204);
    REQUIRE(h.controller.cancel(old).ok());
    REQUIRE(h.dispatcher.enqueue_event(h.event(old, "开始录像", 1), 1100).ok());
    REQUIRE(h.reports.wait(1));
    REQUIRE(h.reports.at(0).outcome == voice::IntentOutcome::REJECTED_CANCELLED);
    REQUIRE(h.controller.complete_cancel(old).ok());
    const auto current = h.session(205);
    REQUIRE(h.dispatcher.enqueue_event(h.event(old, "开始录像", 2), 1100).ok());
    REQUIRE(h.reports.wait(2));
    REQUIRE(h.reports.at(1).outcome == voice::IntentOutcome::REJECTED_STALE);
    h.fixture.clock->advance(101);
    REQUIRE(h.dispatcher.enqueue_event(h.event(current, "开始录像", 3), 1100).ok());
    REQUIRE(h.reports.wait(3));
    REQUIRE(h.reports.at(2).outcome == voice::IntentOutcome::REJECTED_STALE);
    REQUIRE(h.client.count() == 0);
    h.dispatcher.stop();
    return true;
}

bool test_failure_timeout_late() {
    {
        Harness h;
        REQUIRE(h.start());
        h.fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::FAILURE);
        const auto token = h.session(206);
        REQUIRE(h.deliver(h.event(token, "开始录像")).ok() && h.reports.wait(1));
        const auto submission = h.sink.last_submission();
        REQUIRE(submission && submission->accepted());
        REQUIRE(submission->result.get().status.code == protocol::StatusCode::INTERNAL_ERROR);
        REQUIRE(h.client.get_snapshot().recording.value == RecordingState::ERROR);
        REQUIRE(h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1);
        h.dispatcher.stop();
    }
    {
        Harness h;
        REQUIRE(h.start());
        h.fixture.media->set_behavior(CommandType::RECORDING_START, MockBehavior::TIMEOUT);
        const auto token = h.session(207);
        REQUIRE(h.deliver(h.event(token, "开始录像")).ok() && h.reports.wait(1));
        const auto submission = h.sink.last_submission();
        REQUIRE(submission && submission->accepted());
        REQUIRE(wait_until([&] { return h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1; }));
        h.fixture.clock->advance(101);
        REQUIRE(h.fixture.core.poll_deadlines().ok());
        REQUIRE(submission->result.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        REQUIRE(submission->result.get().status.code == protocol::StatusCode::TIMEOUT);
        REQUIRE(h.client.get_snapshot().recording.value == RecordingState::ERROR);
        const auto revision = h.client.get_snapshot().revision;
        REQUIRE(h.fixture.media->complete_pending(token.request_id, protocol::Status::Ok()).ok());
        REQUIRE(wait_until([&] { return h.fixture.core.ignored_late_results() >= 1; }));
        REQUIRE(h.client.get_snapshot().revision == revision);
        REQUIRE(h.client.get_snapshot().recording.value == RecordingState::ERROR);
        h.dispatcher.stop();
    }
    return true;
}

bool test_mapping_and_simulated() {
    Harness h;
    REQUIRE(h.start());
    auto token = h.session(208);
    REQUIRE(h.deliver(h.event(token, "切换前摄")).ok() && h.reports.wait(1));
    auto mapped = h.client.last();
    REQUIRE(mapped && mapped->command_type == CommandType::CAMERA_SELECT);
    REQUIRE(mapped->parameters == CommandParameters({{"camera", "front"}}));
    REQUIRE(h.sink.last_submission()->result.get().status.ok());

    token = h.session(209);
    REQUIRE(h.deliver(h.event(token, "切换后摄")).ok() && h.reports.wait(2));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->parameters == CommandParameters({{"camera", "rear"}}));
    REQUIRE(h.sink.last_submission()->result.get().status.code == protocol::StatusCode::UNAVAILABLE);

    token = h.session(210);
    REQUIRE(h.deliver(h.event(token, "打开蜂鸣器")).ok() && h.reports.wait(3));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->command_type == CommandType::SIM_BUZZER_SET);
    REQUIRE(mapped->parameters == CommandParameters({{"enabled", "true"}}));
    auto submission = h.sink.last_submission();
    REQUIRE(submission && submission->result.get().simulated);
    REQUIRE(h.client.get_snapshot().simulated_buzzer.condition == StateCondition::SIMULATED);
    REQUIRE(h.client.get_snapshot().rtos.value == BinaryState::OFF);

    token = h.session(211);
    REQUIRE(h.deliver(h.event(token, "关闭蜂鸣器")).ok() && h.reports.wait(4));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->parameters == CommandParameters({{"enabled", "false"}}));

    token = h.session(212);
    REQUIRE(h.deliver(h.event(token, "打开LED")).ok() && h.reports.wait(5));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->command_type == CommandType::SIM_LED_SET);
    REQUIRE(mapped->parameters == CommandParameters({{"enabled", "true"}}));

    token = h.session(213);
    REQUIRE(h.deliver(h.event(token, "关闭LED")).ok() && h.reports.wait(6));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->parameters == CommandParameters({{"enabled", "false"}}));

    token = h.session(214);
    REQUIRE(h.deliver(h.event(token, "停止录像")).ok() && h.reports.wait(7));
    mapped = h.client.last();
    REQUIRE(mapped && mapped->command_type == CommandType::RECORDING_STOP);
    REQUIRE(mapped->parameters.empty());

    const auto sent = h.client.count();
    token = h.session(215);
    REQUIRE(h.deliver(h.event(token, "打开摄像头")).ok() && h.reports.wait(8));
    REQUIRE(h.reports.at(7).status.code == protocol::StatusCode::UNSUPPORTED_ACTION);
    REQUIRE(h.client.count() == sent);
    token = h.session(216);
    REQUIRE(h.deliver(h.event(token, "关闭摄像头")).ok() && h.reports.wait(9));
    REQUIRE(h.reports.at(8).outcome == voice::IntentOutcome::NO_MATCH);
    REQUIRE(h.client.count() == sent);
    h.dispatcher.stop();
    return true;
}

bool test_epoch_and_shape() {
    Harness h;
    REQUIRE(h.start());
    const auto token = h.session(217);
    auto invalid = voice::CandidateAction{voice::ActionType::SELECT_CAMERA, voice::CameraId::Front,
                                          voice::ActionSource::RULE, token, 1100, 1};
    invalid.parameter = static_cast<voice::CameraId>(99);
    REQUIRE(h.sink.submit_candidate(invalid).code == protocol::StatusCode::INVALID_ARGUMENT);
    invalid.parameter = voice::CameraId::Front;
    invalid.deadline_ms = 1200;
    REQUIRE(h.sink.submit_candidate(invalid).code == protocol::StatusCode::INVALID_ARGUMENT);
    invalid.deadline_ms = 1100;
    invalid.source = voice::ActionSource::LLM_CANDIDATE;
    REQUIRE(h.sink.submit_candidate(invalid).code == protocol::StatusCode::UNSUPPORTED_ACTION);
    REQUIRE(h.client.count() == 0);

    // A deliberately mismatched adapter/Core epoch cannot silently rewrite the command.
    voice::VoiceSessionController other_controller(76);
    const auto other_token = other_controller.start(218);
    REQUIRE(other_controller.transition(other_token, voice::VoiceSessionState::Recognizing).ok());
    VehicleCommandSinkAdapter other_sink(h.client, h.fixture.clock, 76);
    REQUIRE(other_sink.activate_session(other_token, 1100).ok());
    voice::CandidateAction other{voice::ActionType::START_RECORDING, {}, voice::ActionSource::RULE,
                                 other_token, 1100, 1};
    REQUIRE(other_sink.submit_candidate(other).code == protocol::StatusCode::STALE_EPOCH);
    const auto captured = h.client.last();
    REQUIRE(captured && captured->boot_epoch == 76);
    REQUIRE(h.fixture.media->invocation_count(CommandType::RECORDING_START) == 0);
    h.dispatcher.stop();
    return true;
}

bool test_bounded_queue_and_shutdown() {
    Harness h(1);
    REQUIRE(h.start());
    const auto token = h.session(219);
    const auto final = h.event(token, "开始录像");
    int overflow = 0;
    REQUIRE(h.controller.deliver_event(token, protocol::MessageType::ASR_FINAL, [&] {
        for (int i = 0; i < 5; ++i)
            if (h.dispatcher.enqueue_event(final, 1100).code == protocol::StatusCode::UNAVAILABLE) ++overflow;
    }).ok());
    REQUIRE(overflow >= 1);
    REQUIRE(h.dispatcher.stats().overflow >= 1);
    REQUIRE(h.reports.wait(1));
    h.dispatcher.stop();
    REQUIRE(h.dispatcher.start().ok());
    h.dispatcher.stop();
    REQUIRE(h.dispatcher.enqueue_event(final, 1100).code == protocol::StatusCode::INVALID_STATE);
    return true;
}
}  // namespace

int main() {
    if (!test_normal_ack_result_state() || !test_rejections_and_partial() ||
        !test_duplicate_and_core_replay() || !test_cancel_stale_deadline() ||
        !test_failure_timeout_late() || !test_mapping_and_simulated() ||
        !test_epoch_and_shape() || !test_bounded_queue_and_shutdown()) return 1;
    std::cout << "VOICE_INTENT_CORE_INTEGRATION_TEST_PASS\n";
    return 0;
}
