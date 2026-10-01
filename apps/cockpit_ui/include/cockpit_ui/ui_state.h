#pragma once

#include <string>
#include <string_view>

namespace cockpit::ui {

enum class AvailabilityState {
    Unknown,
    Offline,
    Starting,
    Online,
    Degraded,
    Error,
    Simulated,
    NotReady,
    Unavailable,
    Timeout,
};

enum class StateSource {
    Mock,
    Historical,
    Runtime,
};

struct ServiceStatus {
    AvailabilityState state{AvailabilityState::Unknown};
    StateSource source{StateSource::Mock};
    std::string detail;
};

struct UiState {
    ServiceStatus wifi;
    ServiceStatus camera_front;
    ServiceStatus camera_rear;
    ServiceStatus audio;
    ServiceStatus voice;
    ServiceStatus vision;
    ServiceStatus language_model;
    ServiceStatus rtos;
    ServiceStatus sensor;
    ServiceStatus recording;
    ServiceStatus rtsp;
    ServiceStatus simulated_controls;

    std::string current_camera{"Front"};
    std::string vision_model{"Not loaded"};
    std::string latest_inference{"N/A"};
    std::string inference_rate{"--"};
    std::string voice_session{"Idle / MOCK"};
    std::string asr_state{"NOT READY"};
    std::string latest_asr_text{"N/A"};
    std::string intent_state{"NOT READY"};
    std::string llm_state{"NOT READY"};
    std::string tts_state{"NOT READY"};
    std::string latest_result{"MOCK backend active"};

    std::string cpu{"--"};
    std::string memory{"--"};
    std::string temperature{"--"};
    std::string disk{"--"};

    bool simulated_led_on{false};
    bool simulated_buzzer_on{false};
};

[[nodiscard]] UiState makeMockInitialState();
[[nodiscard]] std::string_view toString(AvailabilityState state) noexcept;
[[nodiscard]] std::string_view toString(StateSource source) noexcept;

}  // namespace cockpit::ui

