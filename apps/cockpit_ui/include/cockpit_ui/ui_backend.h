#pragma once

#include "cockpit_ui/ui_state.h"

#include <cstdint>
#include <functional>
#include <string>

namespace cockpit::ui {

enum class UiCommand {
    SwitchCamera,
    PreviewStart,
    PreviewStop,
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
    VoiceSessionCancel,
    LedSet,
    BuzzerSet,
};

struct UiRequest {
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
    UiCommand command{UiCommand::MediaStop};
    UiResultStatus status{UiResultStatus::Error};
    std::string message;
    bool terminal{true};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == UiResultStatus::Ok || status == UiResultStatus::Accepted;
    }
};

class IUiBackend {
public:
    using StateCallback = std::function<void(UiState)>;
    using ResultCallback = std::function<void(UiResult)>;

    virtual ~IUiBackend() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual UiState currentState() const = 0;
    virtual UiResult submit(const UiRequest& request) = 0;
    virtual void setStateCallback(StateCallback callback) = 0;
    virtual void setResultCallback(ResultCallback callback) = 0;
};

}  // namespace cockpit::ui
