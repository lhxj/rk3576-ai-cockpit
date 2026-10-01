#include "cockpit_ui/mock_ui_backend.h"

#include <utility>

namespace cockpit::ui {

MockUiBackend::MockUiBackend() : state_(makeMockInitialState()) {}

bool MockUiBackend::start() {
    StateCallback callback;
    UiState snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        started_ = true;
        callback = callback_;
        snapshot = state_;
    }
    publish(callback, snapshot);
    return true;
}

void MockUiBackend::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
}

UiState MockUiBackend::currentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

UiResult MockUiBackend::submit(const UiRequest& request) {
    const auto request_id = next_request_id_.fetch_add(1);
    StateCallback callback;
    UiState snapshot;
    UiResult result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) {
            return {request_id, request.command, UiResultStatus::Unavailable,
                    "MOCK backend is stopped", true};
        }
        history_.push_back({request_id, request});

        switch (request.command) {
        case UiCommand::SwitchCamera:
            if (request.argument == "rear") {
                result = unavailable(request_id, request, "Rear camera unavailable (MOCK)");
            } else {
                state_.current_camera = "Front";
                result = accepted(request_id, request,
                                  "Front camera selected in DEMO; no stream opened");
            }
            break;
        case UiCommand::Snapshot:
            result = unavailable(request_id, request, "Snapshot service unavailable (MOCK)");
            break;
        case UiCommand::RecordingStart:
        case UiCommand::RecordingStop:
            result = unavailable(request_id, request, "Recording service unavailable (MOCK)");
            break;
        case UiCommand::RtspStart:
        case UiCommand::RtspStop:
            result = unavailable(request_id, request, "RTSP service unavailable (MOCK)");
            break;
        case UiCommand::VoiceSessionStart:
        case UiCommand::VoiceSessionCancel:
            result = unavailable(request_id, request, "Voice service NOT READY (MOCK)");
            break;
        case UiCommand::MediaPlay:
        case UiCommand::MediaPause:
        case UiCommand::MediaPrevious:
        case UiCommand::MediaNext:
        case UiCommand::MediaStop:
            result = accepted(request_id, request,
                              "Media request recorded by MOCK backend; no media opened");
            break;
        case UiCommand::LedSet:
            state_.simulated_led_on = request.enabled;
            result = accepted(request_id, request, request.enabled ? "SIMULATED LED enabled"
                                                                   : "SIMULATED LED disabled");
            break;
        case UiCommand::BuzzerSet:
            state_.simulated_buzzer_on = request.enabled;
            result = accepted(request_id, request, request.enabled ? "SIMULATED buzzer enabled"
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
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

void MockUiBackend::setResultCallback(ResultCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_callback_ = std::move(callback);
}

std::vector<MockUiBackend::RecordedRequest> MockUiBackend::requestHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

UiResult MockUiBackend::unavailable(std::uint64_t request_id, const UiRequest& request,
                                    std::string message) {
    return {request_id, request.command, UiResultStatus::Unavailable, std::move(message), true};
}

UiResult MockUiBackend::accepted(std::uint64_t request_id, const UiRequest& request,
                                 std::string message) {
    return {request_id, request.command, UiResultStatus::Accepted, std::move(message), true};
}

void MockUiBackend::publish(const StateCallback& callback, const UiState& snapshot) const {
    if (callback) {
        try {
            callback(snapshot);
        } catch (...) {
        }
    }
}

}  // namespace cockpit::ui
