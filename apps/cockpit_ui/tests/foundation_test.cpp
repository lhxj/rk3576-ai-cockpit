#include "cockpit_ui/mock_ui_backend.h"
#include "cockpit_ui/page_id.h"
#include "cockpit_ui/preview_frame_metadata.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

}  // namespace

int main() {
    using namespace cockpit::ui;

    MockUiBackend backend;
    int state_updates = 0;
    backend.setStateCallback([&state_updates](UiState) { ++state_updates; });
    expect(state_updates == 1, "Backend callback must publish the initial state");
    const auto initial = backend.currentState();
    expect(initial.camera_rear.state == AvailabilityState::Offline,
           "Rear camera must default to OFFLINE");
    expect(initial.rtos.state == AvailabilityState::Offline,
           "RTOS must default to OFFLINE");
    expect(initial.sensor.state == AvailabilityState::Offline,
           "MPU/sensor must default to OFFLINE");
    expect(initial.vision.state == AvailabilityState::NotReady,
           "Vision must default to NOT READY");
    expect(initial.voice.state == AvailabilityState::NotReady,
           "Voice must default to NOT READY");
    expect(initial.language_model.state == AvailabilityState::NotReady,
           "Language model must default to NOT READY");
    expect(initial.simulated_controls.state == AvailabilityState::Simulated,
           "Software controls must be explicitly SIMULATED");
    expect(initial.wifi.source == StateSource::Mock,
           "Mock state must identify its source");

    expect(pageIndex(PageId::Home) == 0 && pageIndex(PageId::Camera) == 1 &&
               pageIndex(PageId::Media) == 2 && pageIndex(PageId::Vehicle) == 3 &&
               pageIndex(PageId::Ai) == 4 && pageIndex(PageId::Monitor) == 5 &&
               pageIndex(PageId::Settings) == 6,
           "Page navigation mapping must be stable");

    auto rear = backend.submit({1, UiCommand::SwitchCamera, "rear", false});
    expect(!rear.succeeded() && rear.status == UiResultStatus::Unavailable,
           "Rear camera request must fail as unavailable");
    expect(backend.currentState().camera_rear.state == AvailabilityState::Offline,
           "Failed Rear request must not report success");
    expect(state_updates == 2, "A completed mock request must publish state once");

    auto recording = backend.submit({2, UiCommand::RecordingStart, {}, false});
    expect(!recording.succeeded(), "Unavailable recording must return a failed RESULT");
    expect(backend.currentState().recording.state == AvailabilityState::NotReady,
           "Failed recording RESULT must not mark recording active");

    auto led = backend.submit({3, UiCommand::LedSet, {}, true});
    expect(led.succeeded() && led.message.find("SIMULATED") != std::string::npos,
           "LED request must be explicitly simulated");
    expect(backend.currentState().simulated_led_on,
           "Successful simulated LED request must update mock state");

    const auto history = backend.requestHistory();
    expect(history.size() == 3 && history.front().request_id == 1,
           "Mock backend must record requests in order");

    PreviewFrameMetadata placeholder;
    expect(!placeholder.valid(), "Default preview metadata must not claim a valid frame");
    PreviewFrameMetadata frame{"front", 1632, 1224, "NV12", 7, 2, 123456};
    expect(frame.valid(), "Complete preview metadata must be accepted");

    if (failures != 0) {
        return 1;
    }
    std::cout << "cockpit_ui_foundation_test: PASS (MOCK only)\n";
    return 0;
}
