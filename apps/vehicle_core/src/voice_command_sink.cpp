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
        if (clock_->now_ms() > deadline_ms_)
            return {protocol::StatusCode::EXPIRED, "voice candidate deadline"};
        if (!action.parameters.empty())
            return {protocol::StatusCode::INVALID_ARGUMENT, "voice candidate parameters"};
        command.request_id = action.token.request_id;
        command.session_id = action.token.session_id;
        command.boot_epoch = action.token.boot_epoch;
        command.deadline_ms = deadline_ms_;
        command.source = CommandSource::VOICE;
        switch (action.action_type) {
            case voice::ActionType::OPEN_CAMERA:
                command.command_type = CommandType::CAMERA_SELECT;
                command.parameters = {{"camera", "front"}};
                break;
            case voice::ActionType::START_RECORDING:
                command.command_type = CommandType::RECORDING_START;
                break;
            case voice::ActionType::STOP_RECORDING:
                command.command_type = CommandType::RECORDING_STOP;
                break;
            default:
                return {protocol::StatusCode::INVALID_ARGUMENT, "voice action whitelist"};
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
