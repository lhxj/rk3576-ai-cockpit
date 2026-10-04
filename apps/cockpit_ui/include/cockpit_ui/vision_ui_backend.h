#pragma once

#include "cockpit/infer/vision_runtime.hpp"
#include "cockpit_ui/ui_backend.h"

#include <memory>
#include <mutex>

namespace cockpit::ui {

class VisionUiBackend final : public IUiBackend {
public:
    VisionUiBackend(std::unique_ptr<IUiBackend> base,
                    std::shared_ptr<infer::VisionRuntime> vision);
    ~VisionUiBackend() override;
    VisionUiBackend(const VisionUiBackend&) = delete;
    VisionUiBackend& operator=(const VisionUiBackend&) = delete;

    bool start() override;
    void stop() override;
    UiState currentState() const override;
    UiResult submit(const UiRequest& request) override;
    void setStateCallback(StateCallback callback) override;
    void setResultCallback(ResultCallback callback) override;

    static void applyVision(const infer::VisionRuntimeSnapshot& vision, UiState& state);

private:
    struct CallbackBridge {
        std::mutex mutex;
        VisionUiBackend* owner{nullptr};
    };
    void receiveBaseState(UiState state);
    void receiveBaseResult(UiResult result);
    void receiveVisionUpdate();

    std::unique_ptr<IUiBackend> base_;
    std::shared_ptr<infer::VisionRuntime> vision_;
    std::shared_ptr<CallbackBridge> bridge_;
    mutable std::mutex mutex_;
    StateCallback state_callback_;
    ResultCallback result_callback_;
    bool started_{false};
};

}  // namespace cockpit::ui
