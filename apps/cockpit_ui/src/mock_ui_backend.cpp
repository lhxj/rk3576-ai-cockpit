#include "cockpit_ui/mock_ui_backend.h"

#include <utility>

namespace cockpit::ui {

MockUiBackend::MockUiBackend() : state_(makeMockInitialState()) {}

UiState MockUiBackend::currentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

UiResult MockUiBackend::submit(const UiRequest& request) {
    StateCallback callback;
    UiState snapshot;
    UiResult result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back(request);

        switch (request.command) {
        case UiCommand::SwitchCamera:
            if (request.argument == "rear") {
                result = unavailable(request, "Rear camera unavailable (MOCK)");
            } else {
                state_.current_camera = "Front";
                result = accepted(request, "Front camera selected in DEMO; no stream opened");
            }
            break;
        case UiCommand::Snapshot:
            result = unavailable(request, "Snapshot service unavailable (MOCK)");
            break;
        case UiCommand::RecordingStart:
        case UiCommand::RecordingStop:
            result = unavailable(request, "Recording service unavailable (MOCK)");
            break;
        case UiCommand::RtspStart:
        case UiCommand::RtspStop:
            result = unavailable(request, "RTSP service unavailable (MOCK)");
            break;
        case UiCommand::VoiceSessionStart:
            result = unavailable(request, "Voice service NOT READY (MOCK)");
            break;
        case UiCommand::MediaPlay:
        case UiCommand::MediaPause:
        case UiCommand::MediaPrevious:
        case UiCommand::MediaNext:
        case UiCommand::MediaStop:
            result = accepted(request, "Media request recorded by MOCK backend; no media opened");
            break;
        case UiCommand::LedSet:
            state_.simulated_led_on = request.enabled;
            result = accepted(request, request.enabled ? "SIMULATED LED enabled"
                                                       : "SIMULATED LED disabled");
            break;
        case UiCommand::BuzzerSet:
            state_.simulated_buzzer_on = request.enabled;
            result = accepted(request, request.enabled ? "SIMULATED buzzer enabled"
                                                       : "SIMULATED buzzer disabled");
            break;
        }

        state_.latest_result = result.message;
        callback = callback_;
        snapshot = state_;
    }
    publish(callback, snapshot);
    return result;
}

void MockUiBackend::setStateCallback(StateCallback callback) {
    UiState snapshot;
    StateCallback callback_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
        callback_copy = callback_;
        snapshot = state_;
    }
    publish(callback_copy, snapshot);
}

std::vector<UiRequest> MockUiBackend::requestHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

UiResult MockUiBackend::unavailable(const UiRequest& request, std::string message) {
    return {request.request_id, UiResultStatus::Unavailable, std::move(message)};
}

UiResult MockUiBackend::accepted(const UiRequest& request, std::string message) {
    return {request.request_id, UiResultStatus::Accepted, std::move(message)};
}

void MockUiBackend::publish(const StateCallback& callback, const UiState& snapshot) const {
    if (callback) {
        callback(snapshot);
    }
}

}  // namespace cockpit::ui
