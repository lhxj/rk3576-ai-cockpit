#include "cockpit/media/real_media_service_adapter.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <utility>

namespace cockpit::media {
namespace {

protocol::Status map_status(const MediaStatus& status) {
    using protocol::StatusCode;
    switch (status.code) {
    case MediaStatusCode::Ok: return {StatusCode::OK, status.detail};
    case MediaStatusCode::InvalidArgument:
    case MediaStatusCode::UnsupportedCameraFormat:
    case MediaStatusCode::UnsupportedPlaneLayout:
        return {StatusCode::INVALID_ARGUMENT, status.detail};
    case MediaStatusCode::InvalidState:
    case MediaStatusCode::CameraNotStreaming:
        return {StatusCode::INVALID_STATE, status.detail};
    case MediaStatusCode::Unavailable:
    case MediaStatusCode::NotImplemented:
        return {StatusCode::UNAVAILABLE, status.detail};
    case MediaStatusCode::Timeout: return {StatusCode::TIMEOUT, status.detail};
    case MediaStatusCode::Cancelled: return {StatusCode::CANCELLED, status.detail};
    case MediaStatusCode::IoError:
    case MediaStatusCode::RecordingBackpressure:
    case MediaStatusCode::EncodeError:
        return {StatusCode::INTERNAL_ERROR, status.detail};
    }
    return {StatusCode::INTERNAL_ERROR, status.detail};
}

std::optional<MediaOperation> operation_for(vehicle::CommandType type) {
    switch (type) {
    case vehicle::CommandType::CAMERA_PREVIEW_START: return MediaOperation::PreviewStart;
    case vehicle::CommandType::CAMERA_PREVIEW_STOP: return MediaOperation::PreviewStop;
    case vehicle::CommandType::CAMERA_SNAPSHOT: return MediaOperation::Snapshot;
    case vehicle::CommandType::RECORDING_START: return MediaOperation::RecordingStart;
    case vehicle::CommandType::RECORDING_STOP: return MediaOperation::RecordingStop;
    default: return std::nullopt;
    }
}

const std::string* parameter(const vehicle::VehicleCommand& command, const std::string& key) {
    for (const auto& item : command.parameters)
        if (item.first == key) return &item.second;
    return nullptr;
}

}  // namespace

struct RealMediaServiceAdapter::CompletionState {
    std::mutex mutex;
    std::map<protocol::RequestId, vehicle::AdapterCompletion> pending;
    std::function<void(vehicle::CommandType, vehicle::AdapterResult)> runtime_state_callback;
};

RealMediaServiceAdapter::RealMediaServiceAdapter(std::shared_ptr<MediaService> service)
    : service_(std::move(service)), completions_(std::make_shared<CompletionState>()) {
    const std::weak_ptr<CompletionState> weak = completions_;
    if (service_) {
        service_->set_recording_failure_callback([weak](MediaStatus status) {
            const auto state = weak.lock();
            if (!state) return;
            std::function<void(vehicle::CommandType, vehicle::AdapterResult)> callback;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                callback = state->runtime_state_callback;
            }
            if (callback) {
                callback(vehicle::CommandType::RECORDING_START,
                         {map_status(status), false, vehicle::StateSource::RUNTIME});
            }
        });
    }
}

RealMediaServiceAdapter::~RealMediaServiceAdapter() {
    if (service_) service_->set_recording_failure_callback({});
    cancel_all();
}

bool RealMediaServiceAdapter::supports(vehicle::CommandType type) const {
    switch (type) {
    case vehicle::CommandType::CAMERA_SELECT:
    case vehicle::CommandType::CAMERA_SNAPSHOT:
    case vehicle::CommandType::CAMERA_PREVIEW_START:
    case vehicle::CommandType::CAMERA_PREVIEW_STOP: return true;
    case vehicle::CommandType::RECORDING_START:
    case vehicle::CommandType::RECORDING_STOP:
        return service_ && service_->recording_supported();
    default: return false;
    }
}

void RealMediaServiceAdapter::set_runtime_state_callback(
    std::function<void(vehicle::CommandType, vehicle::AdapterResult)> callback) {
    std::lock_guard<std::mutex> lock(completions_->mutex);
    completions_->runtime_state_callback = std::move(callback);
}

vehicle::DispatchReceipt RealMediaServiceAdapter::dispatch(
    const vehicle::VehicleCommand& command, vehicle::AdapterCompletion completion) {
    if (!service_)
        return {{protocol::StatusCode::UNAVAILABLE, "real media service missing"}, std::nullopt};
    if (command.command_type == vehicle::CommandType::CAMERA_SELECT) {
        const auto* camera = parameter(command, "camera");
        if (camera == nullptr || *camera != "front")
            return {protocol::Status::Ok(),
                    vehicle::AdapterResult{{protocol::StatusCode::UNAVAILABLE,
                                            "rear camera unavailable"},
                                           false, vehicle::StateSource::RUNTIME}};
        return {protocol::Status::Ok(),
                vehicle::AdapterResult{{protocol::StatusCode::OK, "front CAM0 selected"}, false,
                                       vehicle::StateSource::RUNTIME}};
    }
    const auto operation = operation_for(command.command_type);
    if (!operation.has_value())
        return {{protocol::StatusCode::UNAVAILABLE, "NOT_IMPLEMENTED by CAM0 media service"},
                std::nullopt};
    if (!completion)
        return {{protocol::StatusCode::INVALID_ARGUMENT, "media completion callback"},
                std::nullopt};
    {
        std::lock_guard<std::mutex> lock(completions_->mutex);
        completions_->pending[command.request_id] = std::move(completion);
    }
    const auto state = completions_;
    const auto request_id = command.request_id;
    const auto accepted = service_->submit(*operation, [state, request_id](MediaOperationResult result) {
        vehicle::AdapterCompletion callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto found = state->pending.find(request_id);
            if (found == state->pending.end()) return;
            callback = std::move(found->second);
            state->pending.erase(found);
        }
        callback({map_status(result.status), false, vehicle::StateSource::RUNTIME});
    });
    if (!accepted.ok()) {
        std::lock_guard<std::mutex> lock(completions_->mutex);
        completions_->pending.erase(command.request_id);
        return {map_status(accepted), std::nullopt};
    }
    return {protocol::Status::Ok(), std::nullopt};
}

void RealMediaServiceAdapter::cancel_request(protocol::RequestId request_id) {
    std::lock_guard<std::mutex> lock(completions_->mutex);
    completions_->pending.erase(request_id);
}

void RealMediaServiceAdapter::cancel_all() {
    std::lock_guard<std::mutex> lock(completions_->mutex);
    completions_->pending.clear();
}

}  // namespace cockpit::media
