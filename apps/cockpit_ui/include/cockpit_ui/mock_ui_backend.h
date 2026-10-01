#pragma once

#include "cockpit_ui/ui_backend.h"

#include <mutex>
#include <vector>

namespace cockpit::ui {

class MockUiBackend final : public IUiBackend {
public:
    MockUiBackend();

    [[nodiscard]] UiState currentState() const override;
    UiResult submit(const UiRequest& request) override;
    void setStateCallback(StateCallback callback) override;

    [[nodiscard]] std::vector<UiRequest> requestHistory() const;

private:
    UiResult unavailable(const UiRequest& request, std::string message);
    UiResult accepted(const UiRequest& request, std::string message);
    void publish(const StateCallback& callback, const UiState& snapshot) const;

    mutable std::mutex mutex_;
    UiState state_;
    std::vector<UiRequest> history_;
    StateCallback callback_;
};

}  // namespace cockpit::ui
