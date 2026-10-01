#include "cockpit/vehicle/voice_command_sink.hpp"

#include <utility>

namespace cockpit::vehicle {
namespace {
bool same_token(const voice::SessionToken& a, const voice::SessionToken& b) {
    return a.session_id == b.session_id && a.generation == b.generation &&
           a.request_id == b.request_id && a.boot_epoch == b.boot_epoch;
}
}  // namespace

VehicleCommandSinkAdapter::VehicleCommandSinkAdapter(IVehicleCoreClient& client,
    std::shared_ptr<IClock> clock, protocol::BootEpoch boot_epoch)
    : client_(client), clock_(std::move(clock)), boot_epoch_(boot_epoch) {}

protocol::Status VehicleCommandSinkAdapter::activate_session(voice::SessionToken token,
                                                             protocol::Deadline deadline_ms) {
    if (!clock_ || token.session_id == 0 || token.generation == 0 || token.request_id == 0 ||
        token.boot_epoch != boot_epoch_)
        return {protocol::StatusCode::STALE_SESSION, "voice session token"};
    if (deadline_ms == 0 || clock_->now_ms() > deadline_ms)
        return {protocol::StatusCode::EXPIRED, "voice session deadline"};
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = token;
    deadline_ms_ = deadline_ms;
    active_valid_ = true;
    cancelled_ = false;
    last_submission_.reset();
    return protocol::Status::Ok();
}

protocol::Status VehicleCommandSinkAdapter::cancel_session(voice::SessionToken token) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_valid_ || !same_token(active_, token))
        return {protocol::StatusCode::STALE_SESSION, "voice session token"};
    cancelled_ = true;
    return protocol::Status::Ok();
}

protocol::Status VehicleCommandSinkAdapter::submit_candidate(const voice::CandidateAction& action) {
    VehicleCommand command;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_valid_ || !same_token(active_, action.token))
            return {protocol::StatusCode::STALE_SESSION, "voice session token"};
        if (cancelled_) return {protocol::StatusCode::CANCELLED, "voice session cancelled"};
        if (action.source != voice::ActionSource::RULE)
            return {protocol::StatusCode::UNSUPPORTED_ACTION, "only deterministic rule candidates"};
        if (action.asr_sequence == 0 || action.deadline_ms == 0 ||
            action.deadline_ms != deadline_ms_)
            return {protocol::StatusCode::INVALID_ARGUMENT, "voice event deadline/sequence"};
        if (clock_->now_ms() > action.deadline_ms)
            return {protocol::StatusCode::EXPIRED, "voice candidate deadline"};
        command.request_id = action.token.request_id;
        command.session_id = action.token.session_id;
        command.boot_epoch = action.token.boot_epoch;
        command.deadline_ms = action.deadline_ms;
        command.voice_generation = action.token.generation;
        command.asr_sequence = action.asr_sequence;
        command.source = CommandSource::VOICE;
        switch (action.action_type) {
            case voice::ActionType::OPEN_CAMERA:
                return {protocol::StatusCode::UNSUPPORTED_ACTION,
                        "camera preview start has no VehicleCommand"};
            case voice::ActionType::SELECT_CAMERA:
                if (!std::holds_alternative<voice::CameraId>(action.parameter))
                    return {protocol::StatusCode::INVALID_ARGUMENT, "camera parameter"};
                if (std::get<voice::CameraId>(action.parameter) != voice::CameraId::Front &&
                    std::get<voice::CameraId>(action.parameter) != voice::CameraId::Rear)
                    return {protocol::StatusCode::INVALID_ARGUMENT, "camera id"};
                command.command_type = CommandType::CAMERA_SELECT;
                command.parameters = {{"camera", std::get<voice::CameraId>(action.parameter) ==
                    voice::CameraId::Front ? "front" : "rear"}};
                break;
            case voice::ActionType::START_RECORDING:
                if (!std::holds_alternative<std::monostate>(action.parameter))
                    return {protocol::StatusCode::INVALID_ARGUMENT, "recording parameter"};
                command.command_type = CommandType::RECORDING_START;
                break;
            case voice::ActionType::STOP_RECORDING:
                if (!std::holds_alternative<std::monostate>(action.parameter))
                    return {protocol::StatusCode::INVALID_ARGUMENT, "recording parameter"};
                command.command_type = CommandType::RECORDING_STOP;
                break;
            case voice::ActionType::SET_BUZZER:
                if (!std::holds_alternative<bool>(action.parameter))
                    return {protocol::StatusCode::INVALID_ARGUMENT, "buzzer parameter"};
                command.command_type = CommandType::SIM_BUZZER_SET;
                command.parameters = {{"enabled", std::get<bool>(action.parameter) ? "true" : "false"}};
                break;
            case voice::ActionType::SET_LED:
                if (!std::holds_alternative<bool>(action.parameter))
                    return {protocol::StatusCode::INVALID_ARGUMENT, "LED parameter"};
                command.command_type = CommandType::SIM_LED_SET;
                command.parameters = {{"enabled", std::get<bool>(action.parameter) ? "true" : "false"}};
                break;
            default:
                return {protocol::StatusCode::UNSUPPORTED_ACTION, "voice action whitelist"};
        }
    }
    auto submission = client_.send_command(command);
    const auto status = submission.status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_submission_ = std::move(submission);
    }
    return status;
}

std::optional<CommandSubmission> VehicleCommandSinkAdapter::last_submission() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_submission_;
}

}  // namespace cockpit::vehicle
