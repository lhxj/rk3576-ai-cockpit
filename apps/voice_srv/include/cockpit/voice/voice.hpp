#pragma once

#include "cockpit/audio/audio.hpp"
#include "cockpit/protocol/message.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <variant>
#include <string>
#include <utility>
#include <vector>

namespace cockpit::voice {

using VoiceSessionId = protocol::SessionId;
enum class VoiceSessionState {
    Idle, Listening, Recognizing, Understanding, Synthesizing, Playing,
    Cancelling, Completed, Failed
};

struct SessionToken {
    VoiceSessionId session_id{0};
    std::uint64_t generation{0};
    protocol::RequestId request_id{0};
    protocol::BootEpoch boot_epoch{0};
};

enum class AsrEventType { PARTIAL, FINAL, ERROR };
struct AsrEvent {
    AsrEventType type{AsrEventType::PARTIAL};
    SessionToken token;
    std::uint64_t sequence{0};
    std::string text;
    protocol::Status status;
};
using AsrCallback = std::function<void(const AsrEvent&)>;

class IAsrBackend {
public:
    virtual ~IAsrBackend() = default;
    virtual protocol::Status start_session(SessionToken token, const audio::AudioFormat& format,
                                           AsrCallback callback) = 0;
    virtual protocol::Status push_audio(const audio::PcmBuffer& buffer) = 0;
    virtual protocol::Status finish_input(SessionToken token) = 0;
    virtual void cancel(SessionToken token) = 0;
};

struct TtsRequest {
    SessionToken token;
    std::string text;
};
struct TtsEvent {
    SessionToken token;
    audio::PcmBuffer pcm;
    bool finished{false};
    protocol::Status status;
};
using TtsCallback = std::function<void(const TtsEvent&)>;

class ITtsBackend {
public:
    virtual ~ITtsBackend() = default;
    virtual protocol::Status synthesize(const TtsRequest& request, TtsCallback callback) = 0;
    virtual void cancel(SessionToken token) = 0;
};

struct DetectionResult {
    protocol::Status status;
    bool detected{false};
};
class IWakeWordBackend {
public:
    virtual ~IWakeWordBackend() = default;
    virtual DetectionResult detect(const audio::PcmBuffer& buffer) = 0;
};

enum class IntentKind { DETERMINISTIC_COMMAND, GENERAL_QUERY, UNKNOWN };
enum class ActionType {
    OPEN_CAMERA, START_RECORDING, STOP_RECORDING,
    SELECT_CAMERA, SET_BUZZER, SET_LED
};
enum class CameraId { Front, Rear };
using ActionParameter = std::variant<std::monostate, CameraId, bool>;
enum class ActionSource { RULE, LLM_CANDIDATE };

struct CandidateAction {
    ActionType action_type{ActionType::OPEN_CAMERA};
    ActionParameter parameter;
    ActionSource source{ActionSource::RULE};
    SessionToken token;
    protocol::Deadline deadline_ms{0};
    std::uint64_t asr_sequence{0};
};

struct IntentResult {
    IntentKind kind{IntentKind::UNKNOWN};
    CandidateAction candidate;
};

class IIntentRouter {
public:
    virtual ~IIntentRouter() = default;
    virtual IntentResult route(const std::string& text, SessionToken token) const = 0;
};

// Exact keyword fixtures, solely for architecture tests.
class KeywordIntentRouter final : public IIntentRouter {
public:
    IntentResult route(const std::string& text, SessionToken token) const override;
};

// This boundary only accepts a candidate for later vehicle_core validation.
// There is deliberately no camera, audio, RPMsg or hardware method here.
class IVehicleCommandSink {
public:
    virtual ~IVehicleCommandSink() = default;
    virtual protocol::Status submit_candidate(const CandidateAction& action) = 0;
};

class VoiceSessionController {
public:
    explicit VoiceSessionController(protocol::BootEpoch epoch);
    SessionToken start(protocol::RequestId request_id);
    protocol::Status transition(SessionToken token, VoiceSessionState next);
    protocol::Status cancel(SessionToken token);
    protocol::Status complete_cancel(SessionToken token);
    VoiceSessionState state() const;
    SessionToken current() const;
    // Validates the current generation and ASR/intent stage without invoking a callback.
    protocol::Status validate_for_intent(SessionToken token) const;

    // Callback and sink must be quick and non-reentrant. The lock spans delivery,
    // so cancel() returning guarantees no old event/action remains in flight here.
    protocol::Status deliver_event(SessionToken token, protocol::MessageType type,
                                   const std::function<void()>& callback);
    protocol::Status submit_action(const CandidateAction& action, IVehicleCommandSink& sink);

private:
    protocol::Status check_current(SessionToken token) const;
    mutable std::mutex mutex_;
    SessionToken current_;
    VoiceSessionState state_{VoiceSessionState::Idle};
};

}  // namespace cockpit::voice
