#include "cockpit/voice/intent_router.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;

std::vector<IntentRule> make_rules() {
    return {
        {"CAMERA_OPEN_001", VehicleIntent::CAMERA_OPEN, ActionType::OPEN_CAMERA, {},
         "打开摄像头", {"开启摄像头", "打开相机", "开启相机"}},
        {"CAMERA_FRONT_001", VehicleIntent::CAMERA_FRONT, ActionType::SELECT_CAMERA, CameraId::Front,
         "切换前摄", {"切到前摄", "切换前摄像头", "切到前摄像头"}},
        {"CAMERA_REAR_001", VehicleIntent::CAMERA_REAR, ActionType::SELECT_CAMERA, CameraId::Rear,
         "切换后摄", {"切到后摄", "切换后摄像头", "切到后摄像头"}},
        {"RECORDING_START_001", VehicleIntent::RECORDING_START, ActionType::START_RECORDING, {},
         "开始录像", {"开始录制", "开始视频录制"}},
        {"RECORDING_STOP_001", VehicleIntent::RECORDING_STOP, ActionType::STOP_RECORDING, {},
         "停止录像", {"结束录像", "停止录制", "结束录制"}},
        {"BUZZER_ON_001", VehicleIntent::BUZZER_ON, ActionType::SET_BUZZER, true,
         "打开蜂鸣器", {"开启蜂鸣器"}},
        {"BUZZER_OFF_001", VehicleIntent::BUZZER_OFF, ActionType::SET_BUZZER, false,
         "关闭蜂鸣器", {"关掉蜂鸣器"}},
        {"LED_ON_001", VehicleIntent::LED_ON, ActionType::SET_LED, true,
         "打开LED", {"开启LED", "打开灯"}},
        {"LED_OFF_001", VehicleIntent::LED_OFF, ActionType::SET_LED, false,
         "关闭LED", {"关掉LED", "关灯"}},
    };
}

bool utf8_codepoint(const std::string& input, std::size_t& pos, std::uint32_t& value) {
    const auto first = static_cast<unsigned char>(input[pos]);
    if (first < 0x80) { value = first; ++pos; return true; }
    unsigned count = 0;
    std::uint32_t min_value = 0;
    if (first >= 0xC2 && first <= 0xDF) { count = 2; value = first & 0x1F; min_value = 0x80; }
    else if (first >= 0xE0 && first <= 0xEF) { count = 3; value = first & 0x0F; min_value = 0x800; }
    else if (first >= 0xF0 && first <= 0xF4) { count = 4; value = first & 0x07; min_value = 0x10000; }
    else return false;
    if (count > input.size() - pos) return false;
    for (unsigned i = 1; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(input[pos + i]);
        if ((byte & 0xC0) != 0x80) return false;
        value = (value << 6) | (byte & 0x3F);
    }
    pos += count;
    return value >= min_value && value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
}

void append_codepoint(std::string& output, std::uint32_t cp) {
    if (cp <= 0x7F) output.push_back(static_cast<char>(cp));
    else if (cp <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void trim_space(std::string& value) {
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    while (!value.empty() && value.back() == ' ') value.pop_back();
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.compare(0, prefix.size(), prefix) == 0;
}
bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string command_form(std::string text) {
    for (const auto* prefix : {"请帮我", "麻烦你", "帮我", "麻烦", "请"}) {
        if (starts_with(text, prefix)) { text.erase(0, std::char_traits<char>::length(prefix)); break; }
    }
    trim_space(text);
    for (const auto* suffix : {"一下", "吧"}) {
        const std::string marker(suffix);
        if (ends_with(text, marker)) { text.erase(text.size() - marker.size()); break; }
    }
    trim_space(text);
    return text;
}

bool has_negation(const std::string& text) {
    for (const auto* word : {"不", "别", "勿", "取消", "禁止"})
        if (text.find(word) != std::string::npos) return true;
    return false;
}

IntentOutcome invalid_session(StatusCode code) {
    if (code == StatusCode::CANCELLED) return IntentOutcome::REJECTED_CANCELLED;
    if (code == StatusCode::STALE_SESSION || code == StatusCode::STALE_EPOCH)
        return IntentOutcome::REJECTED_STALE;
    return IntentOutcome::INVALID_INPUT;
}
}  // namespace

Status normalize_intent_text(const std::string& input, std::string& output) {
    output.clear();
    if (input.empty() || input.size() > 512)
        return {StatusCode::INVALID_ARGUMENT, "intent text length"};
    for (std::size_t pos = 0; pos < input.size();) {
        std::uint32_t cp = 0;
        if (!utf8_codepoint(input, pos, cp) || cp == 0)
            return {StatusCode::INVALID_ARGUMENT, "intent text UTF-8"};
        if (cp == 0x3000 || cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') cp = ' ';
        else if (cp >= 0xFF01 && cp <= 0xFF5E) cp -= 0xFEE0;
        else if (cp == 0x3002) cp = '.';
        else if (cp == 0x3001) cp = ',';
        if (cp >= 'A' && cp <= 'Z') cp += 'a' - 'A';
        if (cp == ' ') {
            if (!output.empty() && output.back() != ' ') output.push_back(' ');
        } else append_codepoint(output, cp);
    }
    trim_space(output);
    while (!output.empty() && (output.back() == '.' || output.back() == '!' ||
                               output.back() == '?' || output.back() == ',' ||
                               output.back() == ';' || output.back() == ':')) output.pop_back();
    trim_space(output);
    if (output.empty()) return {StatusCode::INVALID_ARGUMENT, "empty intent text"};
    return Status::Ok();
}

RuleTableStats DeterministicIntentRouter::inspect_rules(const std::vector<IntentRule>& rules) {
    RuleTableStats stats;
    stats.rule_count = rules.size();
    std::unordered_map<std::string, std::string> owners;
    std::unordered_set<std::string> ids;
    for (const auto& rule : rules) {
        if (rule.id.empty() || !ids.insert(rule.id).second) ++stats.collision_count;
        stats.alias_count += rule.aliases.size();
        auto inspect_phrase = [&](const std::string& phrase) {
            std::string normalized;
            if (!normalize_intent_text(phrase, normalized).ok()) { ++stats.collision_count; return; }
            normalized = command_form(normalized);
            if (normalized.empty() || has_negation(normalized) || !owners.emplace(normalized, rule.id).second)
                ++stats.collision_count;
        };
        inspect_phrase(rule.canonical);
        for (const auto& alias : rule.aliases) inspect_phrase(alias);
    }
    return stats;
}

DeterministicIntentRouter::DeterministicIntentRouter() : rules_(make_rules()), stats_(inspect_rules(rules_)) {
    if (stats_.collision_count != 0) throw std::logic_error("intent rule collision");
}

IntentMatch DeterministicIntentRouter::match(const IntentInput& input,
                                              const VoiceSessionController& controller,
                                              protocol::Deadline now_ms) const {
    IntentMatch result;
    if (input.event.type != AsrEventType::FINAL || !input.event.status.ok() ||
        input.event.token.session_id == 0 || input.event.token.generation == 0 ||
        input.event.token.request_id == 0 || input.event.token.boot_epoch == 0 ||
        input.deadline_ms == 0 || now_ms == 0) {
        result.outcome = IntentOutcome::INVALID_INPUT;
        return result;
    }
    const auto session = controller.validate_for_intent(input.event.token);
    if (!session.ok()) { result.outcome = invalid_session(session.code); return result; }
    if (now_ms > input.deadline_ms) { result.outcome = IntentOutcome::REJECTED_STALE; return result; }
    if (!normalize_intent_text(input.event.text, result.normalized_text).ok()) {
        result.outcome = IntentOutcome::INVALID_INPUT;
        return result;
    }
    const auto text = command_form(result.normalized_text);
    if (has_negation(text)) { result.outcome = IntentOutcome::REJECTED_NEGATED; return result; }

    auto matched = [&](const IntentRule& rule, ConfidenceClass confidence) {
        result.outcome = IntentOutcome::MATCH;
        result.intent = rule.intent;
        result.matched_rule_id = rule.id;
        result.confidence_class = confidence;
        result.candidate = CandidateAction{rule.action, rule.parameter, ActionSource::RULE,
                                           input.event.token, input.deadline_ms, input.event.sequence};
    };
    for (const auto& rule : rules_) {
        std::string canonical;
        normalize_intent_text(rule.canonical, canonical);
        if (text == canonical) { matched(rule, ConfidenceClass::EXACT); return result; }
        for (const auto& alias : rule.aliases) {
            std::string normalized;
            normalize_intent_text(alias, normalized);
            if (text == normalized) { matched(rule, ConfidenceClass::ALIAS); return result; }
        }
    }
    for (const auto& rule : rules_) {
        const auto check_repeat = [&](const std::string& phrase) {
            for (int repetitions = 2; repetitions <= 3; ++repetitions) {
                std::string repeated;
                for (int i = 0; i < repetitions; ++i) repeated += phrase;
                if (text == repeated) return true;
            }
            return false;
        };
        std::string canonical;
        normalize_intent_text(rule.canonical, canonical);
        if (check_repeat(canonical)) { matched(rule, ConfidenceClass::REPEATED_EXACT); return result; }
        for (const auto& alias : rule.aliases) {
            std::string normalized;
            normalize_intent_text(alias, normalized);
            if (check_repeat(normalized)) { matched(rule, ConfidenceClass::REPEATED_EXACT); return result; }
        }
    }
    // Substrings are used only to reject mixed commands, never to authorize an action.
    std::unordered_set<std::string> contained_rules;
    for (const auto& rule : rules_) {
        auto contains = [&](const std::string& phrase) {
            std::string normalized;
            normalize_intent_text(phrase, normalized);
            return text.find(normalized) != std::string::npos;
        };
        if (contains(rule.canonical)) contained_rules.insert(rule.id);
        for (const auto& alias : rule.aliases)
            if (contains(alias)) contained_rules.insert(rule.id);
    }
    // The current vehicle_core has CAMERA_SELECT but no preview-stop command.
    // Treat a mixed close request as ambiguous without authorizing close itself.
    for (const auto* unsupported : {"关闭摄像头", "关掉摄像头", "关闭相机", "关掉相机"})
        if (text.find(unsupported) != std::string::npos) contained_rules.insert("UNSUPPORTED_CAMERA_CLOSE");
    result.outcome = contained_rules.size() > 1 ? IntentOutcome::REJECTED_AMBIGUOUS : IntentOutcome::NO_MATCH;
    return result;
}

Status DeterministicIntentRouter::dispatch(const IntentMatch& match, VoiceSessionController& controller,
                                            IVehicleCommandSink& sink, protocol::Deadline now_ms) const {
    if (match.outcome != IntentOutcome::MATCH || !match.candidate)
        return {StatusCode::INVALID_STATE, "no matched candidate"};
    if (now_ms == 0 || match.candidate->deadline_ms == 0 || now_ms > match.candidate->deadline_ms)
        return {StatusCode::EXPIRED, "intent deadline"};
    const auto valid = controller.validate_for_intent(match.candidate->token);
    if (!valid.ok()) return valid;
    if (controller.state() == VoiceSessionState::Recognizing) {
        const auto transition = controller.transition(match.candidate->token, VoiceSessionState::Understanding);
        if (!transition.ok()) return transition;
    }
    return controller.submit_action(*match.candidate, sink);
}

const char* intent_outcome_name(IntentOutcome outcome) {
    switch (outcome) {
        case IntentOutcome::MATCH: return "MATCH";
        case IntentOutcome::NO_MATCH: return "NO_MATCH";
        case IntentOutcome::REJECTED_AMBIGUOUS: return "REJECTED_AMBIGUOUS";
        case IntentOutcome::REJECTED_NEGATED: return "REJECTED_NEGATED";
        case IntentOutcome::REJECTED_STALE: return "REJECTED_STALE";
        case IntentOutcome::REJECTED_CANCELLED: return "REJECTED_CANCELLED";
        case IntentOutcome::INVALID_INPUT: return "INVALID_INPUT";
    }
    return "INVALID_INPUT";
}
const char* action_type_name(ActionType action) {
    switch (action) {
        case ActionType::OPEN_CAMERA: return "OPEN_CAMERA";
        case ActionType::SELECT_CAMERA: return "SELECT_CAMERA";
        case ActionType::START_RECORDING: return "START_RECORDING";
        case ActionType::STOP_RECORDING: return "STOP_RECORDING";
        case ActionType::SET_BUZZER: return "SET_BUZZER";
        case ActionType::SET_LED: return "SET_LED";
    }
    return "INVALID_ACTION";
}
const char* confidence_class_name(ConfidenceClass confidence) {
    switch (confidence) {
        case ConfidenceClass::EXACT: return "EXACT";
        case ConfidenceClass::ALIAS: return "ALIAS";
        case ConfidenceClass::REPEATED_EXACT: return "REPEATED_EXACT";
    }
    return "UNKNOWN";
}
}  // namespace cockpit::voice
