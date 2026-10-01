#include "cockpit/voice/intent_router.hpp"

#include <iostream>
#include <random>
#include <string>
#include <variant>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

namespace {
using namespace cockpit;
class Sink final : public voice::IVehicleCommandSink {
public:
    protocol::Status submit_candidate(const voice::CandidateAction& value) override {
        ++count;
        last = value;
        return failure ? protocol::Status{protocol::StatusCode::UNAVAILABLE, "sink unavailable"}
                       : protocol::Status::Ok();
    }
    voice::CandidateAction last;
    int count{0};
    bool failure{false};
};
voice::IntentInput input(const std::string& text, voice::SessionToken token,
                         voice::AsrEventType type = voice::AsrEventType::FINAL) {
    return {{type, token, 7, text, protocol::Status::Ok()}, 2000};
}
voice::SessionToken recognizing(voice::VoiceSessionController& controller) {
    const auto token = controller.start(31);
    controller.transition(token, voice::VoiceSessionState::Recognizing);
    return token;
}
}  // namespace

int main() {
    voice::DeterministicIntentRouter router;
    const auto stats = router.stats();
    CHECK(stats.rule_count == 10);
    CHECK(stats.alias_count == 23);
    CHECK(stats.collision_count == 0);
    std::cout << "INTENT_RULE_STATS rules=" << stats.rule_count << " aliases=" << stats.alias_count
              << " collisions=" << stats.collision_count << '\n';

    // Every maintained canonical and alias is exercised, including typed parameters.
    for (const auto& rule : router.rules()) {
        bool canonical = true;
        for (const auto& phrase : [&] {
                 auto phrases = rule.aliases;
                 phrases.insert(phrases.begin(), rule.canonical);
                 return phrases;
             }()) {
            voice::VoiceSessionController controller(4);
            auto token = recognizing(controller);
            auto result = router.match(input(phrase, token), controller, 1000);
            CHECK(result.outcome == voice::IntentOutcome::MATCH);
            CHECK(result.intent && *result.intent == rule.intent);
            CHECK(result.matched_rule_id == rule.id);
            CHECK(result.confidence_class == (canonical ? voice::ConfidenceClass::EXACT
                                                        : voice::ConfidenceClass::ALIAS));
            CHECK(result.candidate && result.candidate->action_type == rule.action);
            CHECK(result.candidate->parameter == rule.parameter);
            CHECK(result.candidate->token.session_id == token.session_id);
            CHECK(result.candidate->token.generation == token.generation);
            CHECK(result.candidate->token.boot_epoch == token.boot_epoch);
            CHECK(result.candidate->token.request_id == token.request_id);
            CHECK(result.candidate->asr_sequence == 7 && result.candidate->deadline_ms == 2000);
            Sink sink;
            CHECK(router.dispatch(result, controller, sink, 1000).ok());
            CHECK(sink.count == 1 && sink.last.source == voice::ActionSource::RULE);
            CHECK(sink.last.parameter == rule.parameter);
            canonical = false;
        }
    }

    // Normalization is locale-free, bounded and conservative.
    std::string normalized;
    CHECK(voice::normalize_intent_text("  打开摄像头。！ ", normalized).ok());
    CHECK(normalized == "打开摄像头");
    CHECK(voice::normalize_intent_text(" 打开\t\t摄像头 ", normalized).ok());
    CHECK(normalized == "打开 摄像头");
    CHECK(voice::normalize_intent_text("打开ＬＥＤ！", normalized).ok());
    CHECK(normalized == "打开led");
    CHECK(!voice::normalize_intent_text(std::string(513, 'x'), normalized).ok());
    CHECK(!voice::normalize_intent_text(std::string("\xC0\xAF", 2), normalized).ok());

    voice::VoiceSessionController controller(9);
    const auto token = recognizing(controller);
    auto check = [&](const std::string& text, voice::IntentOutcome expected) {
        const auto result = router.match(input(text, token), controller, 1000);
        return result.outcome == expected &&
               (expected == voice::IntentOutcome::MATCH) == result.candidate.has_value();
    };
    for (const auto& phrase : {" 打开摄像头 ", "打开摄像头。", "打开摄像头！", "关闭摄像头",
                               "请打开摄像头", "请帮我打开摄像头", "麻烦你停止录像",
                               "打开LED", "打开led", "打开ＬＥＤ", "请打开摄像头一下"})
        CHECK(check(phrase, voice::IntentOutcome::MATCH));
    for (const auto& phrase : {"不要打开摄像头", "别打开摄像头", "请不要开始录像",
                               "不用打开蜂鸣器", "不要切换后摄", "请别打开摄像头"})
        CHECK(check(phrase, voice::IntentOutcome::REJECTED_NEGATED));
    for (const auto& phrase : {"打开摄像头关闭摄像头", "打开摄像头然后开始录像",
                               "开始录像然后停止录像", "打开摄像头开始录像"})
        CHECK(check(phrase, voice::IntentOutcome::REJECTED_AMBIGUOUS));
    for (const auto& phrase : {"今天天气怎么样", "播放音乐", "你好", "帮我导航",
                               "打开摄像", "打开摄象头", "开始绿象", "重启系统",
                               "关机", "运行shell", "执行命令", "删除文件", "修改配置",
                               "刷固件", "写内存", "开启root", "RUN_SHELL"})
        CHECK(check(phrase, voice::IntentOutcome::NO_MATCH));
    for (const auto& phrase : {"打开摄像头打开摄像头", "开始录像开始录像",
                               "打开摄像头打开摄像头打开摄像头"}) {
        auto result = router.match(input(phrase, token), controller, 1000);
        CHECK(result.outcome == voice::IntentOutcome::MATCH);
        CHECK(result.confidence_class == voice::ConfidenceClass::REPEATED_EXACT);
    }
    CHECK(check("打开摄像头打开摄像头打开摄像头打开摄像头", voice::IntentOutcome::NO_MATCH));

    Sink sink;
    for (const auto& phrase : {"打开摄像", "打开摄像头"}) {
        auto partial = router.match(input(phrase, token, voice::AsrEventType::PARTIAL), controller, 1000);
        CHECK(partial.outcome == voice::IntentOutcome::INVALID_INPUT && !partial.candidate);
        CHECK(router.dispatch(partial, controller, sink, 1000).code == protocol::StatusCode::INVALID_STATE);
    }
    CHECK(sink.count == 0);
    auto expired = router.match(input("开始录像", token), controller, 2001);
    CHECK(expired.outcome == voice::IntentOutcome::REJECTED_STALE && !expired.candidate);
    auto valid = router.match(input("开始录像", token), controller, 1000);
    CHECK(router.dispatch(valid, controller, sink, 2001).code == protocol::StatusCode::EXPIRED);
    CHECK(sink.count == 0);
    sink.failure = true;
    CHECK(router.dispatch(valid, controller, sink, 1000).code == protocol::StatusCode::UNAVAILABLE);
    CHECK(sink.count == 1); // precisely one attempt, no retry or LLM fallback
    auto wrong_parameter = valid.candidate.value();
    wrong_parameter.parameter = voice::CameraId::Rear;
    CHECK(controller.submit_action(wrong_parameter, sink).code == protocol::StatusCode::INVALID_ARGUMENT);
    CHECK(sink.count == 1);
    auto invalid_camera = router.match(input("切换后摄", token), controller, 1000).candidate.value();
    invalid_camera.parameter = static_cast<voice::CameraId>(99);
    CHECK(controller.submit_action(invalid_camera, sink).code == protocol::StatusCode::INVALID_ARGUMENT);
    CHECK(sink.count == 1);
    auto malformed = input("开始录像", token);
    malformed.event.status = {protocol::StatusCode::INTERNAL_ERROR, "ASR error"};
    CHECK(router.match(malformed, controller, 1000).outcome == voice::IntentOutcome::INVALID_INPUT);

    voice::VoiceSessionController cancelled(2);
    auto old = recognizing(cancelled);
    CHECK(cancelled.cancel(old).ok());
    CHECK(router.match(input("打开摄像头", old), cancelled, 1000).outcome ==
          voice::IntentOutcome::REJECTED_CANCELLED);
    CHECK(cancelled.complete_cancel(old).ok());
    auto current = recognizing(cancelled);
    CHECK(router.match(input("打开摄像头", old), cancelled, 1000).outcome ==
          voice::IntentOutcome::REJECTED_STALE);
    auto wrong_epoch = current;
    ++wrong_epoch.boot_epoch;
    CHECK(router.match(input("打开摄像头", wrong_epoch), cancelled, 1000).outcome ==
          voice::IntentOutcome::REJECTED_STALE);
    auto before_cancel = router.match(input("打开摄像头", current), cancelled, 1000);
    CHECK(cancelled.cancel(current).ok());
    CHECK(router.dispatch(before_cancel, cancelled, sink, 1000).code == protocol::StatusCode::CANCELLED);
    CHECK(sink.count == 1);

    auto duplicated = router.rules();
    duplicated[1].aliases.push_back(" 打开摄像头。 ");
    CHECK(voice::DeterministicIntentRouter::inspect_rules(duplicated).collision_count == 1);

    std::mt19937 rng(12345);
    const std::string alphabet = "abcdefghijklmxyz_0123456789";
    for (int i = 0; i < 1000; ++i) {
        std::string random(24, ' ');
        for (char& c : random) c = alphabet[rng() % alphabet.size()];
        auto match = router.match(input(random, token), controller, 1000);
        CHECK(match.outcome == voice::IntentOutcome::NO_MATCH && !match.candidate);
    }
    return 0;
}
