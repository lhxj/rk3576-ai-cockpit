#include "cockpit/voice/intent_router.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace {
using namespace cockpit;
class RecordingSink final : public voice::IVehicleCommandSink {
public:
    protocol::Status submit_candidate(const voice::CandidateAction& candidate) override {
        last = candidate;
        ++count;
        return protocol::Status::Ok();
    }
    voice::CandidateAction last;
    int count{0};
};

struct RunResult {
    voice::IntentMatch match;
    protocol::Status dispatch;
    int submissions{0};
};

RunResult run(const voice::DeterministicIntentRouter& router, const std::string& text) {
    voice::VoiceSessionController controller(1);
    const auto token = controller.start(1);
    controller.transition(token, voice::VoiceSessionState::Recognizing);
    const voice::IntentInput input{{voice::AsrEventType::FINAL, token, 1, text,
                                    protocol::Status::Ok()}, 2000};
    auto match = router.match(input, controller, 1000);
    RecordingSink sink;
    protocol::Status dispatch;
    if (match.outcome == voice::IntentOutcome::MATCH)
        dispatch = router.dispatch(match, controller, sink, 1000);
    return {match, dispatch, sink.count};
}

int run_fixture(const voice::DeterministicIntentRouter& router, const std::string& path) {
    std::ifstream file(path);
    if (!file) { std::cerr << "fixture unavailable\n"; return 2; }
    std::string line;
    std::size_t line_no = 0;
    std::size_t cases = 0;
    while (std::getline(file, line)) {
        ++line_no;
        if (line.empty() || line.front() == '#') continue;
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos || line.find('\t', second + 1) != std::string::npos) {
            std::cerr << "invalid fixture line " << line_no << '\n'; return 2;
        }
        const auto input = line.substr(0, first);
        const auto outcome = line.substr(first + 1, second - first - 1);
        const auto action = line.substr(second + 1);
        const auto result = run(router, input);
        const std::string actual_action = result.match.candidate
            ? voice::action_type_name(result.match.candidate->action_type) : "-";
        if (voice::intent_outcome_name(result.match.outcome) != outcome || actual_action != action ||
            result.submissions != (result.match.outcome == voice::IntentOutcome::MATCH ? 1 : 0)) {
            std::cerr << "fixture mismatch line=" << line_no << " result="
                      << voice::intent_outcome_name(result.match.outcome)
                      << " action=" << actual_action << '\n';
            return 1;
        }
        ++cases;
    }
    std::cout << "INTENT_FIXTURE_PASS cases=" << cases << '\n';
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: cockpit_intent_test --text TEXT | --fixture FILE\n"
                  << "Text only; candidate is recorded in memory. No hardware access.\n";
        return 0;
    }
    if (argc != 3) { std::cerr << "Expected --text TEXT or --fixture FILE\n"; return 2; }
    const std::string option(argv[1]);
    voice::DeterministicIntentRouter router;
    if (option == "--fixture") return run_fixture(router, argv[2]);
    if (option != "--text") return 2;
    const auto result = run(router, argv[2]);
    std::cout << "INPUT=" << argv[2] << "\nNORMALIZED=" << result.match.normalized_text
              << "\nRESULT=" << voice::intent_outcome_name(result.match.outcome) << '\n';
    if (result.match.candidate) {
        std::string parameter = "none";
        if (const auto camera = std::get_if<voice::CameraId>(&result.match.candidate->parameter))
            parameter = *camera == voice::CameraId::Front ? "CameraId::Front" : "CameraId::Rear";
        else if (const auto enabled = std::get_if<bool>(&result.match.candidate->parameter))
            parameter = *enabled ? "enabled=true" : "enabled=false";
        std::cout << "RULE=" << result.match.matched_rule_id
                  << "\nCONFIDENCE_CLASS=" << voice::confidence_class_name(result.match.confidence_class)
                  << "\nACTION=" << voice::action_type_name(result.match.candidate->action_type)
                  << "\nPARAMETER=" << parameter
                  << "\nACTION_SUBMITTED=" << result.submissions << '\n';
    }
    return result.dispatch.ok() ? 0 : 1;
}
