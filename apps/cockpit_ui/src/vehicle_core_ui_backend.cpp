#include "cockpit_ui/vehicle_core_ui_backend.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <utility>

namespace cockpit::ui {
namespace {

StateSource mapSource(vehicle::StateSource source) {
    switch (source) {
    case vehicle::StateSource::UNKNOWN:
        return StateSource::Unknown;
    case vehicle::StateSource::RUNTIME:
        return StateSource::Runtime;
    case vehicle::StateSource::HISTORICAL:
        return StateSource::Historical;
    case vehicle::StateSource::MOCK:
        return StateSource::Mock;
    }
    return StateSource::Unknown;
}

AvailabilityState mapCondition(vehicle::StateCondition condition) {
    switch (condition) {
    case vehicle::StateCondition::UNKNOWN:
        return AvailabilityState::Unknown;
    case vehicle::StateCondition::OFFLINE:
        return AvailabilityState::Offline;
    case vehicle::StateCondition::STARTING:
        return AvailabilityState::Starting;
    case vehicle::StateCondition::ONLINE:
        return AvailabilityState::Online;
    case vehicle::StateCondition::DEGRADED:
        return AvailabilityState::Degraded;
    case vehicle::StateCondition::ERROR:
        return AvailabilityState::Error;
    case vehicle::StateCondition::SIMULATED:
        return AvailabilityState::Simulated;
    }
    return AvailabilityState::Unknown;
}

AvailabilityState mapHealth(vehicle::ServiceHealth health) {
    switch (health) {
    case vehicle::ServiceHealth::UNKNOWN:
        return AvailabilityState::Unknown;
    case vehicle::ServiceHealth::STARTING:
        return AvailabilityState::Starting;
    case vehicle::ServiceHealth::ONLINE:
        return AvailabilityState::Online;
    case vehicle::ServiceHealth::DEGRADED:
        return AvailabilityState::Degraded;
    case vehicle::ServiceHealth::OFFLINE:
        return AvailabilityState::Offline;
    case vehicle::ServiceHealth::ERROR:
        return AvailabilityState::Error;
    }
    return AvailabilityState::Unknown;
}

ServiceStatus serviceStatus(const vehicle::ServiceState& value, const char* detail) {
    return {mapHealth(value.health), mapSource(value.source), detail};
}

template <typename T>
ServiceStatus stateStatus(const vehicle::StateValue<T>& value, std::string detail) {
    return {mapCondition(value.condition), mapSource(value.source), std::move(detail)};
}

std::string recordingName(vehicle::RecordingState state) {
    switch (state) {
    case vehicle::RecordingState::STOPPED:
        return "Stopped";
    case vehicle::RecordingState::STARTING:
        return "Starting";
    case vehicle::RecordingState::RECORDING:
        return "Recording";
    case vehicle::RecordingState::STOPPING:
        return "Stopping";
    case vehicle::RecordingState::ERROR:
        return "Error";
    }
    return "Unknown";
}

std::string previewName(vehicle::PreviewState state) {
    switch (state) {
    case vehicle::PreviewState::STOPPED: return "Stopped";
    case vehicle::PreviewState::STARTING: return "Starting";
    case vehicle::PreviewState::STREAMING: return "Streaming";
    case vehicle::PreviewState::STOPPING: return "Stopping";
    case vehicle::PreviewState::ERROR: return "Error";
    }
    return "Unknown";
}

std::string mediaName(vehicle::MediaState state) {
    switch (state) {
    case vehicle::MediaState::STOPPED:
        return "Stopped";
    case vehicle::MediaState::PLAYING:
        return "Playing";
    case vehicle::MediaState::PAUSED:
        return "Paused";
    case vehicle::MediaState::ERROR:
        return "Error";
    }
    return "Unknown";
}

std::string voiceName(vehicle::VoiceState state) {
    switch (state) {
    case vehicle::VoiceState::IDLE:
        return "Idle";
    case vehicle::VoiceState::STARTING:
        return "Starting";
    case vehicle::VoiceState::ACTIVE:
        return "Active";
    case vehicle::VoiceState::CANCELLING:
        return "Cancelling";
    case vehicle::VoiceState::ERROR:
        return "Error";
    }
    return "Unknown";
}

UiResultStatus resultStatus(protocol::StatusCode code) {
    switch (code) {
    case protocol::StatusCode::OK:
        return UiResultStatus::Ok;
    case protocol::StatusCode::TIMEOUT:
    case protocol::StatusCode::EXPIRED:
        return UiResultStatus::Timeout;
    case protocol::StatusCode::UNAVAILABLE:
        return UiResultStatus::Unavailable;
    case protocol::StatusCode::INVALID_ARGUMENT:
    case protocol::StatusCode::INVALID_STATE:
    case protocol::StatusCode::CANCELLED:
    case protocol::StatusCode::MALFORMED:
    case protocol::StatusCode::UNSUPPORTED_VERSION:
    case protocol::StatusCode::DUPLICATE_REQUEST:
    case protocol::StatusCode::STALE_SESSION:
    case protocol::StatusCode::STALE_EPOCH:
        return UiResultStatus::Rejected;
    case protocol::StatusCode::INTERNAL_ERROR:
        return UiResultStatus::Error;
    }
    return UiResultStatus::Error;
}

const char* commandName(UiCommand command) {
    switch (command) {
    case UiCommand::SwitchCamera:
        return "camera select";
    case UiCommand::PreviewStart:
        return "preview start";
    case UiCommand::PreviewStop:
        return "preview stop";
    case UiCommand::Snapshot:
        return "snapshot";
    case UiCommand::RecordingStart:
        return "recording start";
    case UiCommand::RecordingStop:
        return "recording stop";
    case UiCommand::RtspStart:
        return "RTSP start";
    case UiCommand::RtspStop:
        return "RTSP stop";
    case UiCommand::MediaPlay:
        return "media play";
    case UiCommand::MediaPause:
        return "media pause";
    case UiCommand::MediaPrevious:
        return "media previous";
    case UiCommand::MediaNext:
        return "media next";
    case UiCommand::MediaStop:
        return "media stop";
    case UiCommand::VoiceSessionStart:
        return "voice start";
    case UiCommand::VoiceSessionCancel:
        return "voice cancel";
    case UiCommand::LedSet:
        return "simulated LED";
    case UiCommand::BuzzerSet:
        return "simulated buzzer";
    }
    return "command";
}

}  // namespace

UiState mapVehicleState(const vehicle::VehicleState& state) {
    UiState mapped;
    mapped.revision = state.revision;
    const auto media_service_source =
        state.services.at(static_cast<std::size_t>(vehicle::ServiceDomain::MEDIA)).source;
    mapped.backend_mode = media_service_source == vehicle::StateSource::RUNTIME
                              ? "CORE / CAM0 REAL"
                              : "CORE / MOCK SERVICES";
    mapped.wifi = stateStatus(state.wifi, "Vehicle Core canonical Wi-Fi state");
    mapped.camera_front = stateStatus(state.front_camera, "Front camera availability from Vehicle Core");
    if (state.front_camera.value == vehicle::CameraAvailability::UNAVAILABLE)
        mapped.camera_front.state = AvailabilityState::Unavailable;
    mapped.camera_rear = stateStatus(state.rear_camera, "Rear camera availability from Vehicle Core");
    if (state.rear_camera.value == vehicle::CameraAvailability::UNAVAILABLE)
        mapped.camera_rear.state = AvailabilityState::Unavailable;
    mapped.audio = stateStatus(state.audio, "Audio state from Vehicle Core");
    mapped.voice = stateStatus(state.voice, "Voice session state from Vehicle Core");
    mapped.vision = stateStatus(state.vision, "Vision state from Vehicle Core");
    mapped.language_model = stateStatus(state.language_model, "Language model state from Vehicle Core");
    mapped.rtos = stateStatus(state.rtos, "RTOS business state; AMP remains unverified");
    mapped.sensor = stateStatus(state.sensor, "MPU6050 is not integrated");
    mapped.preview = stateStatus(state.preview, previewName(state.preview.value));
    mapped.recording = stateStatus(state.recording, recordingName(state.recording.value));
    mapped.rtsp = stateStatus(state.rtsp,
                              state.rtsp.value == vehicle::BinaryState::ON ? "On" : "Off");
    mapped.simulated_controls = {AvailabilityState::Simulated, StateSource::Mock,
                                 "LED and buzzer remain software simulations"};

    const auto service = [&](vehicle::ServiceDomain domain) -> const vehicle::ServiceState& {
        return state.services.at(static_cast<std::size_t>(domain));
    };
    mapped.media_service = serviceStatus(service(vehicle::ServiceDomain::MEDIA), "Media adapter health");
    mapped.voice_service = serviceStatus(service(vehicle::ServiceDomain::VOICE), "Voice adapter health");
    mapped.infer_service = serviceStatus(service(vehicle::ServiceDomain::INFER), "Infer adapter health");
    mapped.rtos_service = serviceStatus(service(vehicle::ServiceDomain::RTOS), "RTOS adapter health");
    mapped.system_service = serviceStatus(service(vehicle::ServiceDomain::SYSTEM), "System adapter health");

    switch (state.selected_camera.value) {
    case vehicle::CameraSelection::FRONT:
        mapped.current_camera = "Front";
        break;
    case vehicle::CameraSelection::REAR:
        mapped.current_camera = "Rear";
        break;
    case vehicle::CameraSelection::NONE:
        mapped.current_camera = "None";
        break;
    }
    mapped.recording_state = recordingName(state.recording.value);
    mapped.preview_state = previewName(state.preview.value);
    mapped.rtsp_state = state.rtsp.value == vehicle::BinaryState::ON ? "On" : "Off";
    mapped.media_state = mediaName(state.media.value);
    mapped.voice_session = voiceName(state.voice.value);
    mapped.asr_state = "NOT READY";
    mapped.intent_state = "NOT READY";
    mapped.llm_state = "NOT READY";
    mapped.tts_state = "NOT READY";
    mapped.latest_result = "Vehicle Core connected; no command RESULT yet";
    mapped.simulated_led_on = state.simulated_led.value == vehicle::BinaryState::ON;
    mapped.simulated_buzzer_on = state.simulated_buzzer.value == vehicle::BinaryState::ON;
    return mapped;
}

bool RevisionedStateProjector::apply(const vehicle::VehicleState& state, UiState& projected) {
    if (initialized_ && state.revision <= last_revision_) return false;
    projected = mapVehicleState(state);
    last_revision_ = state.revision;
    initialized_ = true;
    return true;
}

VehicleCoreUiBackend::VehicleCoreUiBackend(vehicle::IVehicleCoreClient& client,
                                           std::shared_ptr<vehicle::IClock> clock,
                                           VehicleCoreUiBackendOptions options)
    : client_(client), clock_(std::move(clock)), options_(options),
      callback_gate_(std::make_shared<CallbackGate>()), canonical_state_(makeMockInitialState()) {
    canonical_state_.backend_mode = "CORE / STARTING";
}

VehicleCoreUiBackend::~VehicleCoreUiBackend() { stop(); }

bool VehicleCoreUiBackend::start() {
    StateCallback initial_callback;
    UiState initial_snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_ || !clock_ || options_.pending_capacity == 0 ||
            options_.command_timeout_ms == 0 || client_.boot_epoch() == 0)
            return false;
        stopping_ = false;
        projector_.apply(client_.get_snapshot(), canonical_state_);
        started_ = true;
        initial_callback = state_callback_;
        initial_snapshot = snapshotWithPendingLocked();
    }
    // Render a complete snapshot before relying on incremental state events.
    publishState(initial_snapshot, initial_callback);
    {
        std::lock_guard<std::mutex> gate_lock(callback_gate_->mutex);
        callback_gate_->owner = this;
    }
    try {
        result_worker_ = std::thread(&VehicleCoreUiBackend::resultLoop, this);
    } catch (...) {
        stop();
        return false;
    }

    const std::weak_ptr<CallbackGate> weak_gate = callback_gate_;
    const auto status = client_.subscribe_state([weak_gate](const vehicle::VehicleState& state) {
        const auto gate = weak_gate.lock();
        if (!gate) return;
        std::lock_guard<std::mutex> lock(gate->mutex);
        if (gate->owner != nullptr) gate->owner->receiveState(state);
    });
    if (!status.ok()) {
        stop();
        return false;
    }

    // Close the snapshot/subscribe race. Equal or older revisions are discarded.
    receiveState(client_.get_snapshot());
    return true;
}

void VehicleCoreUiBackend::stop() {
    std::lock_guard<std::mutex> submit_lock(submit_mutex_);
    {
        std::lock_guard<std::mutex> gate_lock(callback_gate_->mutex);
        callback_gate_->owner = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_ && !result_worker_.joinable()) return;
        stopping_ = true;
    }
    wake_.notify_all();
    if (result_worker_.joinable()) result_worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.clear();
    started_ = false;
}

UiState VehicleCoreUiBackend::currentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshotWithPendingLocked();
}

UiResult VehicleCoreUiBackend::submit(const UiRequest& request) {
    std::lock_guard<std::mutex> submit_lock(submit_mutex_);
    const auto request_id = next_request_id_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_ || stopping_)
            return {request_id, request.command, UiResultStatus::Unavailable,
                    "Vehicle Core backend is stopped", true};
        if (pending_.size() >= options_.pending_capacity)
            return {request_id, request.command, UiResultStatus::Unavailable,
                    "Vehicle Core UI pending queue is full", true};
    }

    auto command = makeCommand(request_id, request);
    auto submission = client_.send_command(command);
    if (!submission.status.ok()) {
        const auto detail = submission.status.detail.empty() ? "command rejected"
                                                             : submission.status.detail;
        return {request_id, request.command, resultStatus(submission.status.code), detail, true};
    }
    if ((!submission.accepted() && !submission.replayed()) || !submission.result.valid())
        return {request_id, request.command, UiResultStatus::Error,
                "Vehicle Core returned no terminal RESULT future", true};

    StateCallback callback;
    UiState snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return {request_id, request.command, UiResultStatus::Unavailable,
                    "Vehicle Core UI backend is stopping", true};
        pending_.push_back({request_id, request.command, submission.result});
        callback = state_callback_;
        snapshot = snapshotWithPendingLocked();
    }
    wake_.notify_one();
    publishState(snapshot, callback);
    return {request_id, request.command, UiResultStatus::Accepted,
            submission.replayed() ? "Request replayed; awaiting RESULT"
                                  : "ACK accepted; awaiting RESULT",
            false};
}

void VehicleCoreUiBackend::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_callback_ = std::move(callback);
}

void VehicleCoreUiBackend::setResultCallback(ResultCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_callback_ = std::move(callback);
}

vehicle::VehicleCommand VehicleCoreUiBackend::makeCommand(std::uint64_t request_id,
                                                           const UiRequest& request) {
    vehicle::VehicleCommand command;
    command.request_id = request_id;
    command.boot_epoch = client_.boot_epoch();
    command.deadline_ms = clock_->now_ms() + options_.command_timeout_ms;
    command.source = vehicle::CommandSource::UI;

    switch (request.command) {
    case UiCommand::SwitchCamera:
        command.command_type = vehicle::CommandType::CAMERA_SELECT;
        command.parameters = {{"camera", request.argument}};
        break;
    case UiCommand::PreviewStart:
        command.command_type = vehicle::CommandType::CAMERA_PREVIEW_START;
        break;
    case UiCommand::PreviewStop:
        command.command_type = vehicle::CommandType::CAMERA_PREVIEW_STOP;
        break;
    case UiCommand::Snapshot:
        command.command_type = vehicle::CommandType::CAMERA_SNAPSHOT;
        break;
    case UiCommand::RecordingStart:
        command.command_type = vehicle::CommandType::RECORDING_START;
        break;
    case UiCommand::RecordingStop:
        command.command_type = vehicle::CommandType::RECORDING_STOP;
        break;
    case UiCommand::RtspStart:
        command.command_type = vehicle::CommandType::RTSP_START;
        break;
    case UiCommand::RtspStop:
        command.command_type = vehicle::CommandType::RTSP_STOP;
        break;
    case UiCommand::MediaPlay:
        command.command_type = vehicle::CommandType::MEDIA_PLAY;
        if (!request.argument.empty()) command.parameters = {{"item", request.argument}};
        break;
    case UiCommand::MediaPause:
        command.command_type = vehicle::CommandType::MEDIA_PAUSE;
        break;
    case UiCommand::MediaPrevious:
        command.command_type = vehicle::CommandType::MEDIA_PREVIOUS;
        break;
    case UiCommand::MediaNext:
        command.command_type = vehicle::CommandType::MEDIA_NEXT;
        break;
    case UiCommand::MediaStop:
        command.command_type = vehicle::CommandType::MEDIA_STOP;
        break;
    case UiCommand::VoiceSessionStart:
        command.command_type = vehicle::CommandType::VOICE_SESSION_START;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (voice_session_id_ == 0) voice_session_id_ = request_id;
            command.session_id = voice_session_id_;
        }
        break;
    case UiCommand::VoiceSessionCancel:
        command.command_type = vehicle::CommandType::VOICE_SESSION_CANCEL;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            command.session_id = voice_session_id_ == 0 ? request_id : voice_session_id_;
        }
        break;
    case UiCommand::LedSet:
        command.command_type = vehicle::CommandType::SIM_LED_SET;
        command.parameters = {{"enabled", request.enabled ? "true" : "false"}};
        break;
    case UiCommand::BuzzerSet:
        command.command_type = vehicle::CommandType::SIM_BUZZER_SET;
        command.parameters = {{"enabled", request.enabled ? "true" : "false"}};
        break;
    }
    return command;
}

void VehicleCoreUiBackend::receiveState(const vehicle::VehicleState& state) {
    StateCallback callback;
    UiState snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        UiState projected;
        if (!projector_.apply(state, projected)) return;
        projected.latest_result = canonical_state_.latest_result;
        canonical_state_ = std::move(projected);
        callback = state_callback_;
        snapshot = snapshotWithPendingLocked();
    }
    publishState(snapshot, callback);
}

void VehicleCoreUiBackend::resultLoop() {
    for (;;) {
        std::vector<PendingResult> ready;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (pending_.empty()) {
                wake_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
            } else {
                wake_.wait_for(lock, std::chrono::milliseconds(2), [this] { return stopping_; });
            }
            if (stopping_) break;
            auto item = pending_.begin();
            while (item != pending_.end()) {
                if (item->future.wait_for(std::chrono::milliseconds(0)) ==
                    std::future_status::ready) {
                    ready.push_back(*item);
                    item = pending_.erase(item);
                } else {
                    ++item;
                }
            }
        }
        for (auto& pending : ready) complete(std::move(pending));
    }
}

void VehicleCoreUiBackend::complete(PendingResult pending) {
    const auto core_result = pending.future.get();
    const auto status = resultStatus(core_result.status.code);
    std::ostringstream message;
    message << commandName(pending.command) << ": ";
    if (core_result.status.detail.empty())
        message << (core_result.status.ok() ? "completed" : "failed");
    else
        message << core_result.status.detail;
    if (core_result.simulated) message << " (SIMULATED)";
    UiResult result{pending.request_id, pending.command, status, message.str(), true};

    StateCallback state_callback;
    ResultCallback result_callback;
    UiState snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        canonical_state_.latest_result = result.message;
        if (pending.command == UiCommand::VoiceSessionCancel ||
            (pending.command == UiCommand::VoiceSessionStart && !core_result.status.ok()))
            voice_session_id_ = 0;
        state_callback = state_callback_;
        result_callback = result_callback_;
        snapshot = snapshotWithPendingLocked();
    }
    publishState(snapshot, state_callback);
    publishResult(result, result_callback);
}

UiState VehicleCoreUiBackend::snapshotWithPendingLocked() const {
    UiState state = canonical_state_;
    for (const auto& item : pending_) {
        const auto detail = std::string("ACK accepted; request #") +
                            std::to_string(item.request_id) + " awaiting RESULT";
        switch (item.command) {
        case UiCommand::PreviewStart:
        case UiCommand::PreviewStop:
            state.preview_pending = true;
            state.preview = {AvailabilityState::Starting, StateSource::Runtime, detail};
            break;
        case UiCommand::RecordingStart:
        case UiCommand::RecordingStop:
            state.recording_pending = true;
            state.recording = {AvailabilityState::Starting, StateSource::Runtime, detail};
            break;
        case UiCommand::RtspStart:
        case UiCommand::RtspStop:
            state.rtsp_pending = true;
            state.rtsp = {AvailabilityState::Starting, StateSource::Runtime, detail};
            break;
        case UiCommand::VoiceSessionStart:
        case UiCommand::VoiceSessionCancel:
            state.voice_pending = true;
            state.voice = {AvailabilityState::Starting, StateSource::Runtime, detail};
            break;
        default:
            break;
        }
    }
    return state;
}

void VehicleCoreUiBackend::publishState(const UiState& state,
                                        const StateCallback& callback) const {
    if (callback) {
        try {
            callback(state);
        } catch (...) {
        }
    }
}

void VehicleCoreUiBackend::publishResult(const UiResult& result,
                                         const ResultCallback& callback) const {
    if (callback) {
        try {
            callback(result);
        } catch (...) {
        }
    }
}

}  // namespace cockpit::ui
