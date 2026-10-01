#pragma once

#include "cockpit_ui/ui_backend.h"

#include <mutex>
#include <atomic>
#include <vector>

namespace cockpit::ui {

class MockUiBackend final : public IUiBackend {
public:
    MockUiBackend();

    bool start() override;
    void stop() override;
    [[nodiscard]] UiState currentState() const override;
    UiResult submit(const UiRequest& request) override;
    void setStateCallback(StateCallback callback) override;
    void setResultCallback(ResultCallback callback) override;

    struct RecordedRequest {
        std::uint64_t request_id{0};
        UiRequest request;
    };
    [[nodiscard]] std::vector<RecordedRequest> requestHistory() const;

private:
    UiResult unavailable(std::uint64_t request_id, const UiRequest& request, std::string message);
    UiResult accepted(std::uint64_t request_id, const UiRequest& request, std::string message);
    void publish(const StateCallback& callback, const UiState& snapshot) const;

    mutable std::mutex mutex_;
    UiState state_;
    std::vector<RecordedRequest> history_;
    StateCallback callback_;
    ResultCallback result_callback_;
    std::atomic<std::uint64_t> next_request_id_{1};
    bool started_{false};
};

}  // namespace cockpit::ui
