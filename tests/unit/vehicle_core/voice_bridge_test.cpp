#include "test_support.hpp"

#include "cockpit/vehicle/voice_command_sink.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace cockpit::vehicle;
    using namespace cockpit::vehicle::test;
    Fixture fixture;
    CHECK(fixture.start_status.ok());
    VehicleCommandSinkAdapter sink(fixture.client, fixture.clock, 77);
    voice::VoiceSessionController controller(77);
    auto token = controller.start(200);
    CHECK(controller.transition(token, voice::VoiceSessionState::Recognizing).ok());
    CHECK(controller.transition(token, voice::VoiceSessionState::Understanding).ok());
    CHECK(sink.activate_session(token, 1100).ok());
    voice::KeywordIntentRouter router;
    auto intent = router.route("开始录像", token);
    CHECK(intent.kind == voice::IntentKind::DETERMINISTIC_COMMAND);
    CHECK(controller.submit_action(intent.candidate, sink).ok());
    auto submission = sink.last_submission();
    CHECK(submission.has_value() && submission->accepted());
    CHECK(submission->result.get().status.ok());
    CHECK(fixture.media->invocation_count(CommandType::RECORDING_START) == 1);

    auto invalid = intent.candidate;
    invalid.token.request_id = 201;
    invalid.action_type = static_cast<voice::ActionType>(999);
    CHECK(sink.submit_candidate(invalid).code == protocol::StatusCode::STALE_SESSION);
    invalid.token = token;
    invalid.action_type = static_cast<voice::ActionType>(999);
    CHECK(sink.submit_candidate(invalid).code == protocol::StatusCode::INVALID_ARGUMENT);
    invalid = intent.candidate;
    invalid.parameters = {{"shell", "RUN_SHELL"}};
    CHECK(sink.submit_candidate(invalid).code == protocol::StatusCode::INVALID_ARGUMENT);

    auto expired_token = controller.start(201);
    CHECK(controller.transition(expired_token, voice::VoiceSessionState::Recognizing).ok());
    CHECK(controller.transition(expired_token, voice::VoiceSessionState::Understanding).ok());
    CHECK(sink.activate_session(expired_token, 1010).ok());
    fixture.clock->advance(11);
    auto expired_action = router.route("停止录像", expired_token).candidate;
    CHECK(sink.submit_candidate(expired_action).code == protocol::StatusCode::EXPIRED);

    auto cancelled_token = controller.start(202);
    CHECK(controller.transition(cancelled_token, voice::VoiceSessionState::Recognizing).ok());
    CHECK(controller.transition(cancelled_token, voice::VoiceSessionState::Understanding).ok());
    CHECK(sink.activate_session(cancelled_token, 1200).ok());
    CHECK(sink.cancel_session(cancelled_token).ok());
    auto cancelled_action = router.route("打开摄像头", cancelled_token).candidate;
    CHECK(sink.submit_candidate(cancelled_action).code == protocol::StatusCode::CANCELLED);

    fixture.voice->set_behavior(CommandType::VOICE_SESSION_START, MockBehavior::TIMEOUT);
    auto voice_start = fixture.command(210, CommandType::VOICE_SESSION_START);
    voice_start.session_id = 50;
    auto active = fixture.client.send_command(voice_start);
    CHECK(active.accepted());
    CHECK(wait_until([&] { return fixture.client.get_snapshot().voice.value == VoiceState::STARTING; }));
    auto voice_cancel = fixture.command(211, CommandType::VOICE_SESSION_CANCEL);
    voice_cancel.session_id = 50;
    auto cancel = fixture.client.send_command(voice_cancel);
    CHECK(cancel.accepted());
    CHECK(active.result.get().status.code == protocol::StatusCode::CANCELLED);
    CHECK(cancel.result.get().status.ok());
    CHECK(fixture.client.get_snapshot().voice.value == VoiceState::IDLE);
    const auto revision = fixture.client.get_snapshot().revision;
    CHECK(fixture.voice->complete_pending(210, protocol::Status::Ok()).ok());
    CHECK(wait_until([&] { return fixture.core.ignored_late_results() >= 1; }));
    CHECK(fixture.client.get_snapshot().voice.value == VoiceState::IDLE);
    CHECK(fixture.client.get_snapshot().revision == revision);
    return 0;
}
