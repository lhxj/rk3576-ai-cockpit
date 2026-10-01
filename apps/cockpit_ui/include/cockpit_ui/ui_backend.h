#pragma once

#include "cockpit_ui/ui_state.h"

#include <cstdint>
#include <functional>
#include <string>

namespace cockpit::ui {

enum class UiCommand {
    SwitchCamera,
    Snapshot,
    RecordingStart,
    RecordingStop,
    RtspStart,
    RtspStop,
    MediaPlay,
    MediaPause,
    MediaPrevious,
    MediaNext,
    MediaStop,
    VoiceSessionStart,
    LedSet,
    BuzzerSet,
};

struct UiRequest {
    std::uint64_t request_id{0};
    UiCommand command{UiCommand::MediaStop};
    std::string argument;
    bool enabled{false};
};

enum class UiResultStatus {
    Ok,
    Accepted,
    Unavailable,
    Rejected,
    Error,
    Timeout,
};

struct UiResult {
    std::uint64_t request_id{0};
    UiResultStatus status{UiResultStatus::Error};
    std::string message;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == UiResultStatus::Ok || status == UiResultStatus::Accepted;
    }
};

class IUiBackend {
public:
    using StateCallback = std::function<void(UiState)>;

    virtual ~IUiBackend() = default;
    [[nodiscard]] virtual UiState currentState() const = 0;
    virtual UiResult submit(const UiRequest& request) = 0;
    virtual void setStateCallback(StateCallback callback) = 0;
};

}  // namespace cockpit::ui
