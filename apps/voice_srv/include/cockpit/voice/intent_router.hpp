#pragma once

#include "cockpit/voice/voice.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cockpit::voice {

enum class VehicleIntent {
    CAMERA_OPEN, CAMERA_CLOSE, CAMERA_FRONT, CAMERA_REAR,
    RECORDING_START, RECORDING_STOP, BUZZER_ON, BUZZER_OFF, LED_ON, LED_OFF
};
enum class IntentOutcome {
    MATCH, NO_MATCH, REJECTED_AMBIGUOUS, REJECTED_NEGATED,
    REJECTED_STALE, REJECTED_CANCELLED, INVALID_INPUT
};
enum class ConfidenceClass { EXACT, ALIAS, REPEATED_EXACT };

struct IntentRule {
    std::string id;
    VehicleIntent intent;
    ActionType action;
    ActionParameter parameter;
    std::string canonical;
    std::vector<std::string> aliases;
};

struct IntentInput {
    AsrEvent event;
    protocol::Deadline deadline_ms{0};
};

struct IntentMatch {
    IntentOutcome outcome{IntentOutcome::NO_MATCH};
    std::string normalized_text;
    std::optional<VehicleIntent> intent;
    std::optional<CandidateAction> candidate;
    std::string matched_rule_id;
    ConfidenceClass confidence_class{ConfidenceClass::EXACT};
};

struct RuleTableStats {
    std::size_t rule_count{0};
    std::size_t alias_count{0};
    std::size_t collision_count{0};
};

// UTF-8 and ASCII normalization only. Invalid UTF-8 and oversized input fail closed.
protocol::Status normalize_intent_text(const std::string& input, std::string& output);
const char* intent_outcome_name(IntentOutcome outcome);
const char* action_type_name(ActionType action);
const char* confidence_class_name(ConfidenceClass confidence);

class DeterministicIntentRouter final {
public:
    DeterministicIntentRouter();
    const std::vector<IntentRule>& rules() const { return rules_; }
    RuleTableStats stats() const { return stats_; }
    static RuleTableStats inspect_rules(const std::vector<IntentRule>& rules);

    // Matching is side-effect free. now_ms uses the protocol's Unix-ms clock domain.
    IntentMatch match(const IntentInput& input, const VoiceSessionController& controller,
                      protocol::Deadline now_ms) const;
    // One submission attempt. A failed sink is returned unchanged; no retry or LLM fallback.
    protocol::Status dispatch(const IntentMatch& match, VoiceSessionController& controller,
                              IVehicleCommandSink& sink, protocol::Deadline now_ms) const;

private:
    std::vector<IntentRule> rules_;
    RuleTableStats stats_;
};

}  // namespace cockpit::voice
