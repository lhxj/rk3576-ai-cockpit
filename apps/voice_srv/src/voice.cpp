#include "cockpit/voice/voice.hpp"

#include <limits>

namespace cockpit::voice {

VoiceSessionController::VoiceSessionController(protocol::BootEpoch epoch) {
    current_.boot_epoch = epoch;
}

IntentResult KeywordIntentRouter::route(const std::string& text, SessionToken token) const {
    IntentResult result;
    result.candidate.token = token;
    if (text == "打开摄像头") result.candidate.action_type = ActionType::OPEN_CAMERA;
    else if (text == "开始录像") result.candidate.action_type = ActionType::START_RECORDING;
    else if (text == "停止录像") result.candidate.action_type = ActionType::STOP_RECORDING;
    else { result.kind = text.empty() ? IntentKind::UNKNOWN : IntentKind::GENERAL_QUERY; return result; }
    result.kind = IntentKind::DETERMINISTIC_COMMAND;
    return result;
}

SessionToken VoiceSessionController::start(protocol::RequestId request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (request_id == 0 || current_.boot_epoch == 0 ||
        current_.generation == std::numeric_limits<std::uint64_t>::max()) return {};
    ++current_.generation;
    ++current_.session_id;
    current_.request_id = request_id;
    state_ = VoiceSessionState::Listening;
    return current_;
}

protocol::Status VoiceSessionController::check_current(SessionToken token) const {
    if (token.session_id == 0 || token.session_id != current_.session_id ||
        token.generation != current_.generation || token.request_id != current_.request_id ||
        token.boot_epoch != current_.boot_epoch)
        return {protocol::StatusCode::STALE_SESSION, "session token"};
    if (state_ == VoiceSessionState::Cancelling || state_ == VoiceSessionState::Completed ||
        state_ == VoiceSessionState::Failed)
        return {protocol::StatusCode::CANCELLED, "session terminal"};
    return protocol::Status::Ok();
}

protocol::Status VoiceSessionController::transition(SessionToken token, VoiceSessionState next) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto valid = check_current(token);
    if (!valid.ok()) return valid;
    bool allowed = false;
    switch (state_) {
        case VoiceSessionState::Listening: allowed = next == VoiceSessionState::Recognizing; break;
        case VoiceSessionState::Recognizing: allowed = next == VoiceSessionState::Understanding; break;
        case VoiceSessionState::Understanding:
            allowed = next == VoiceSessionState::Synthesizing || next == VoiceSessionState::Completed; break;
        case VoiceSessionState::Synthesizing: allowed = next == VoiceSessionState::Playing; break;
        case VoiceSessionState::Playing: allowed = next == VoiceSessionState::Completed; break;
        default: break;
    }
    if (next == VoiceSessionState::Failed) allowed = true;
    if (!allowed) return {protocol::StatusCode::INVALID_STATE, "voice transition"};
    state_ = next;
    return protocol::Status::Ok();
}

protocol::Status VoiceSessionController::cancel(SessionToken token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto valid = check_current(token);
    if (!valid.ok()) return valid;
    state_ = VoiceSessionState::Cancelling;
    return protocol::Status::Ok();
}

protocol::Status VoiceSessionController::complete_cancel(SessionToken token) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (token.session_id != current_.session_id || token.generation != current_.generation ||
        token.request_id != current_.request_id || token.boot_epoch != current_.boot_epoch ||
        state_ != VoiceSessionState::Cancelling)
        return {protocol::StatusCode::INVALID_STATE, "no active cancellation"};
    state_ = VoiceSessionState::Completed;
    return protocol::Status::Ok();
}

VoiceSessionState VoiceSessionController::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

SessionToken VoiceSessionController::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

protocol::Status VoiceSessionController::deliver_event(SessionToken token, protocol::MessageType type,
                                                        const std::function<void()>& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto valid = check_current(token);
    if (!valid.ok()) return valid;
    switch (type) {
        case protocol::MessageType::ASR_PARTIAL:
        case protocol::MessageType::ASR_FINAL:
        case protocol::MessageType::ASR_ERROR:
            if (state_ != VoiceSessionState::Recognizing)
                return {protocol::StatusCode::INVALID_STATE, "ASR event stage"};
            break;
        case protocol::MessageType::LLM_CHUNK:
        case protocol::MessageType::LLM_RESULT:
            if (state_ != VoiceSessionState::Understanding)
                return {protocol::StatusCode::INVALID_STATE, "LLM event stage"};
            break;
        case protocol::MessageType::TTS_STARTED:
            if (state_ != VoiceSessionState::Synthesizing)
                return {protocol::StatusCode::INVALID_STATE, "TTS start stage"};
            break;
        case protocol::MessageType::TTS_FINISHED:
            if (state_ != VoiceSessionState::Playing)
                return {protocol::StatusCode::INVALID_STATE, "TTS finish stage"};
            break;
        default: return {protocol::StatusCode::INVALID_ARGUMENT, "event type"};
    }
    if (!callback) return {protocol::StatusCode::INVALID_ARGUMENT, "callback"};
    callback();
    return protocol::Status::Ok();
}

protocol::Status VoiceSessionController::submit_action(const CandidateAction& action,
                                                        IVehicleCommandSink& sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto valid = check_current(action.token);
    if (!valid.ok()) return valid;
    if (state_ != VoiceSessionState::Understanding)
        return {protocol::StatusCode::INVALID_STATE, "intent stage"};
    return sink.submit_candidate(action);
}

}  // namespace cockpit::voice
