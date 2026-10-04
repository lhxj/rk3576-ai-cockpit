#include "cockpit_ui/vision_ui_backend.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace cockpit::ui {

VisionUiBackend::VisionUiBackend(std::unique_ptr<IUiBackend> base,
                                 std::shared_ptr<infer::VisionRuntime> vision)
    : base_(std::move(base)), vision_(std::move(vision)),
      bridge_(std::make_shared<CallbackBridge>()) {}

VisionUiBackend::~VisionUiBackend() { stop(); }

bool VisionUiBackend::start() {
    if (!base_ || !vision_) return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_) return true;
    }
    const std::weak_ptr<CallbackBridge> weak = bridge_;
    {
        std::lock_guard<std::mutex> lock(bridge_->mutex);
        bridge_->owner = this;
    }
    base_->setStateCallback([weak](UiState state) {
        if (const auto bridge = weak.lock()) {
            std::lock_guard<std::mutex> lock(bridge->mutex);
            if (bridge->owner) bridge->owner->receiveBaseState(std::move(state));
        }
    });
    base_->setResultCallback([weak](UiResult result) {
        if (const auto bridge = weak.lock()) {
            std::lock_guard<std::mutex> lock(bridge->mutex);
            if (bridge->owner) bridge->owner->receiveBaseResult(std::move(result));
        }
    });
    vision_->set_update_callback([weak] {
        if (const auto bridge = weak.lock()) {
            std::lock_guard<std::mutex> lock(bridge->mutex);
            if (bridge->owner) bridge->owner->receiveVisionUpdate();
        }
    });
    const bool started = base_->start();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        started_ = started;
    }
    if (started) receiveVisionUpdate();
    return started;
}

void VisionUiBackend::stop() {
    {
        std::lock_guard<std::mutex> lock(bridge_->mutex);
        bridge_->owner = nullptr;
    }
    if (vision_) vision_->set_update_callback({});
    if (base_) {
        base_->setStateCallback({});
        base_->setResultCallback({});
        base_->stop();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
}

UiState VisionUiBackend::currentState() const {
    UiState state = base_ ? base_->currentState() : UiState{};
    if (vision_) applyVision(vision_->snapshot(), state);
    return state;
}

UiResult VisionUiBackend::submit(const UiRequest& request) {
    return base_ ? base_->submit(request)
                 : UiResult{0, request.command, UiResultStatus::Unavailable,
                            "base backend unavailable", true};
}

void VisionUiBackend::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_callback_ = std::move(callback);
}

void VisionUiBackend::setResultCallback(ResultCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_callback_ = std::move(callback);
}

void VisionUiBackend::applyVision(const infer::VisionRuntimeSnapshot& vision,
                                  UiState& state) {
    state.backend_mode = "CORE+VISION";
    state.vision.source = StateSource::Runtime;
    state.infer_service.source = StateSource::Runtime;
    state.vision_model = vision.backend.model_name.empty()
                             ? "Not loaded" : vision.backend.model_name;
    switch (vision.state) {
    case infer::VisionRuntimeState::NOT_READY:
        state.vision.state = AvailabilityState::NotReady;
        state.infer_service.state = AvailabilityState::NotReady;
        state.vision.detail = "RKNN vision NOT_READY";
        break;
    case infer::VisionRuntimeState::LOADING:
        state.vision.state = AvailabilityState::Starting;
        state.infer_service.state = AvailabilityState::Starting;
        state.vision.detail = "RKNN model LOADING";
        break;
    case infer::VisionRuntimeState::READY:
        state.vision.state = AvailabilityState::Online;
        state.infer_service.state = AvailabilityState::Online;
        state.vision.detail = "RKNN model READY; waiting for CAM0 frame";
        break;
    case infer::VisionRuntimeState::RUNNING:
        state.vision.state = AvailabilityState::Online;
        state.infer_service.state = AvailabilityState::Online;
        state.vision.detail = "RKNN vision RUNNING";
        break;
    case infer::VisionRuntimeState::ERROR:
        state.vision.state = AvailabilityState::Error;
        state.infer_service.state = AvailabilityState::Error;
        state.vision.detail = "RKNN vision ERROR";
        break;
    }
    state.infer_service.detail = state.vision.detail;
    std::ostringstream rate;
    rate << std::fixed << std::setprecision(2) << vision.metrics.vision_fps << " FPS";
    state.inference_rate = rate.str();
    if (vision.latest_result) {
        state.current_camera = vision.latest_result->camera_id;
        std::ostringstream summary;
        if (!vision.latest_result->classifications.empty()) {
            const auto& best = vision.latest_result->classifications.front();
            summary << best.label << " (class " << best.class_id << ", "
                    << std::fixed << std::setprecision(3) << best.confidence << ')';
        } else if (!vision.latest_result->detections.empty()) {
            const auto& best = vision.latest_result->detections.front();
            summary << best.label << " " << std::fixed << std::setprecision(3)
                    << best.confidence;
        } else {
            summary << "No classification/detection";
        }
        summary << " · e" << vision.latest_result->stream_epoch
                << " s" << vision.latest_result->frame_sequence;
        state.latest_inference = summary.str();
    } else {
        state.latest_inference = "Waiting for real CAM0 inference";
    }
}

void VisionUiBackend::receiveBaseState(UiState state) {
    applyVision(vision_->snapshot(), state);
    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = state_callback_;
    }
    if (callback) callback(std::move(state));
}

void VisionUiBackend::receiveBaseResult(UiResult result) {
    ResultCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = result_callback_;
    }
    if (callback) callback(std::move(result));
}

void VisionUiBackend::receiveVisionUpdate() {
    auto state = currentState();
    StateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = state_callback_;
    }
    if (callback) callback(std::move(state));
}

}  // namespace cockpit::ui
