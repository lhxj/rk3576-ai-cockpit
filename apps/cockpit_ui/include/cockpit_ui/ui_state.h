#pragma once

#include <string>
#include <string_view>
#include <cstdint>

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
    Stopping,
};

enum class StateSource {
    Unknown,
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
    std::uint64_t revision{0};
    std::string backend_mode{"MOCK"};
    ServiceStatus wifi;
    ServiceStatus camera_front;
    ServiceStatus camera_rear;
    ServiceStatus preview;
    ServiceStatus audio;
    ServiceStatus voice;
    ServiceStatus vision;
    ServiceStatus language_model;
    ServiceStatus rtos;
    ServiceStatus sensor;
    ServiceStatus recording;
    ServiceStatus rtsp;
    ServiceStatus simulated_controls;
    ServiceStatus media_service;
    ServiceStatus voice_service;
    ServiceStatus infer_service;
    ServiceStatus rtos_service;
    ServiceStatus system_service;

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
    std::string media_state{"Stopped"};
    std::string recording_state{"Not ready"};
    std::string preview_state{"Stopped"};
    std::string rtsp_state{"Not ready"};

    std::string cpu{"--"};
    std::string memory{"--"};
    std::string temperature{"--"};
    std::string disk{"--"};

    bool simulated_led_on{false};
    bool simulated_buzzer_on{false};
    bool preview_pending{false};
    bool recording_pending{false};
    bool rtsp_pending{false};
    bool voice_pending{false};
};

[[nodiscard]] UiState makeMockInitialState();
[[nodiscard]] std::string_view toString(AvailabilityState state) noexcept;
[[nodiscard]] std::string_view toString(StateSource source) noexcept;

}  // namespace cockpit::ui
