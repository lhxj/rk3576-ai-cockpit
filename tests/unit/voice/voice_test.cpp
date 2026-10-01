#include "cockpit/voice/voice.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

class Sink final : public cockpit::voice::IVehicleCommandSink {
public:
    cockpit::protocol::Status submit_candidate(const cockpit::voice::CandidateAction&) override {
        ++count;
        return cockpit::protocol::Status::Ok();
    }
    int count{0};
};

int main() {
    using namespace cockpit;
    voice::VoiceSessionController controller(9);
    auto old = controller.start(1000);
    CHECK(old.session_id != 0);
    CHECK(old.boot_epoch == 9);
    CHECK(controller.state() == voice::VoiceSessionState::Listening);
    CHECK(controller.transition(old, voice::VoiceSessionState::Recognizing).ok());
    CHECK(controller.transition(old, voice::VoiceSessionState::Understanding).ok());
    CHECK(controller.transition(old, voice::VoiceSessionState::Playing).code == protocol::StatusCode::INVALID_STATE);
    voice::KeywordIntentRouter router;
    auto command = router.route("打开摄像头", old);
    CHECK(command.kind == voice::IntentKind::DETERMINISTIC_COMMAND);
    CHECK(router.route("天气怎么样", old).kind == voice::IntentKind::GENERAL_QUERY);
    CHECK(router.route("", old).kind == voice::IntentKind::UNKNOWN);
    Sink sink;
    CHECK(controller.submit_action(command.candidate, sink).ok());
    CHECK(sink.count == 1);

    CHECK(controller.cancel(old).ok());
    CHECK(controller.state() == voice::VoiceSessionState::Cancelling);
    int delivered = 0;
    CHECK(controller.deliver_event(old, protocol::MessageType::LLM_CHUNK, [&] { ++delivered; }).code == protocol::StatusCode::CANCELLED);
    CHECK(controller.complete_cancel(old).ok());
    auto now = controller.start(1001);
    CHECK(now.session_id != old.session_id && now.generation != old.generation);
    for (auto type : {protocol::MessageType::LLM_CHUNK, protocol::MessageType::LLM_RESULT,
                      protocol::MessageType::TTS_STARTED, protocol::MessageType::TTS_FINISHED}) {
        CHECK(controller.deliver_event(old, type, [&] { ++delivered; }).code == protocol::StatusCode::STALE_SESSION);
    }
    CHECK(controller.submit_action(command.candidate, sink).code == protocol::StatusCode::STALE_SESSION);
    CHECK(delivered == 0 && sink.count == 1);
    CHECK(controller.transition(now, voice::VoiceSessionState::Recognizing).ok());
    CHECK(controller.transition(now, voice::VoiceSessionState::Understanding).ok());
    CHECK(controller.deliver_event(now, protocol::MessageType::LLM_CHUNK, [&] { ++delivered; }).ok());
    auto wrong_epoch = now;
    wrong_epoch.boot_epoch = 8;
    CHECK(controller.deliver_event(wrong_epoch, protocol::MessageType::LLM_RESULT,
                                   [&] { ++delivered; }).code == protocol::StatusCode::STALE_SESSION);
    CHECK(delivered == 1);
    return 0;
}
