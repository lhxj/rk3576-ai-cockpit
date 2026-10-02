#include "cockpit_ui/ui_state.h"

namespace cockpit::ui {

UiState makeMockInitialState() {
    UiState state;
    state.backend_mode = "MOCK";
    state.wifi = {AvailabilityState::Degraded, StateSource::Mock,
                  "DEMO: historical Wi-Fi PASS; current runtime is not queried"};
    state.camera_front = {
        AvailabilityState::Degraded,
        StateSource::Mock,
        "DEMO: historical CAM0 capture PASS; no preview backend is connected",
    };
    state.camera_rear = {
        AvailabilityState::Offline,
        StateSource::Mock,
        "Rear camera disabled; replacement connection component is pending",
    };
    state.preview = {AvailabilityState::NotReady, StateSource::Mock,
                     "Preview service unavailable"};
    state.audio = {
        AvailabilityState::Degraded,
        StateSource::Mock,
        "DEMO: historical capture/playback PASS; no audio service is connected",
    };
    state.voice = {AvailabilityState::NotReady, StateSource::Mock,
                   "Voice service NOT READY"};
    state.vision = {AvailabilityState::NotReady, StateSource::Mock,
                    "Vision service NOT READY"};
    state.language_model = {AvailabilityState::NotReady, StateSource::Mock,
                            "Language model NOT READY"};
    state.rtos = {AvailabilityState::Offline, StateSource::Mock,
                  "AMP/RT-Thread has not been verified"};
    state.sensor = {AvailabilityState::Offline, StateSource::Mock,
                    "MPU6050 has not been integrated"};
    state.recording = {AvailabilityState::NotReady, StateSource::Mock,
                       "Recording service unavailable"};
    state.rtsp = {AvailabilityState::NotReady, StateSource::Mock,
                  "RTSP service unavailable"};
    state.simulated_controls = {
        AvailabilityState::Simulated,
        StateSource::Mock,
        "LED and buzzer controls are software simulation only",
    };
    state.media_service = {AvailabilityState::NotReady, StateSource::Mock,
                           "media service is not connected"};
    state.voice_service = {AvailabilityState::NotReady, StateSource::Mock,
                           "voice service is not connected"};
    state.infer_service = {AvailabilityState::NotReady, StateSource::Mock,
                           "infer service is not connected"};
    state.rtos_service = {AvailabilityState::Offline, StateSource::Mock,
                          "RTOS service is offline"};
    state.system_service = {AvailabilityState::Online, StateSource::Mock,
                            "local mock backend"};
    return state;
}

std::string_view toString(AvailabilityState state) noexcept {
    switch (state) {
    case AvailabilityState::Unknown:
        return "UNKNOWN";
    case AvailabilityState::Offline:
        return "OFFLINE";
    case AvailabilityState::Starting:
        return "STARTING";
    case AvailabilityState::Online:
        return "ONLINE";
    case AvailabilityState::Degraded:
        return "AVAILABLE*";
    case AvailabilityState::Error:
        return "ERROR";
    case AvailabilityState::Simulated:
        return "SIMULATED";
    case AvailabilityState::NotReady:
        return "NOT READY";
    case AvailabilityState::Unavailable:
        return "UNAVAILABLE";
    case AvailabilityState::Timeout:
        return "TIMEOUT";
    case AvailabilityState::Stopping:
        return "STOPPING";
    }
    return "UNKNOWN";
}

std::string_view toString(StateSource source) noexcept {
    switch (source) {
    case StateSource::Unknown:
        return "UNKNOWN";
    case StateSource::Mock:
        return "MOCK";
    case StateSource::Historical:
        return "HISTORICAL";
    case StateSource::Runtime:
        return "RUNTIME";
    }
    return "UNKNOWN";
}

}  // namespace cockpit::ui
