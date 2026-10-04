#include "cockpit/infer/rknn_vision_backend.hpp"

#include <rknn_api.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

namespace cockpit::infer {
namespace {
using protocol::Status;
using protocol::StatusCode;
using clock_type = std::chrono::steady_clock;

Status rknn_error(const char* operation, int code) {
    return {StatusCode::INTERNAL_ERROR,
            std::string(operation) + " failed: " + std::to_string(code)};
}

std::string tensor_shape(const rknn_tensor_attr& attribute) {
    std::ostringstream output;
    for (std::uint32_t index = 0; index < attribute.n_dims; ++index) {
        if (index) output << 'x';
        output << attribute.dims[index];
    }
    return output.str();
}

struct SchedulerRelease {
    IInferenceScheduler& scheduler;
    ~SchedulerRelease() { scheduler.release(ModelKind::Vision); }
};
}  // namespace

RknnVisionBackend::RknnVisionBackend(IInferenceScheduler& scheduler,
                                     RknnVisionBackendConfig config)
    : scheduler_(scheduler), config_(std::move(config)) {}

RknnVisionBackend::~RknnVisionBackend() { unload(); }

Status RknnVisionBackend::load() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Unloaded)
            return {StatusCode::INVALID_STATE, "RKNN vision already loaded"};
        if (config_.model_path.empty() || config_.top_k == 0)
            return {StatusCode::INVALID_ARGUMENT, "RKNN model configuration"};
        state_ = ModelState::Loading;
    }

    std::ifstream file(config_.model_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return {StatusCode::UNAVAILABLE, "RKNN model open: " + config_.model_path};
    }
    const auto size = file.tellg();
    if (size <= 0 || static_cast<std::uint64_t>(size) >
                         std::numeric_limits<std::uint32_t>::max()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return {StatusCode::INVALID_ARGUMENT, "RKNN model size"};
    }
    std::vector<std::uint8_t> model(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(model.data()), size)) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return {StatusCode::INTERNAL_ERROR, "RKNN model read"};
    }

    const auto load_start = clock_type::now();
    rknn_context context = 0;
    int result = rknn_init(&context, model.data(), static_cast<std::uint32_t>(model.size()),
                           0, nullptr);
    if (result != RKNN_SUCC) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return rknn_error("rknn_init", result);
    }

    rknn_input_output_num io{};
    result = rknn_query(context, RKNN_QUERY_IN_OUT_NUM, &io, sizeof(io));
    if (result != RKNN_SUCC || io.n_input != 1 || io.n_output != 1) {
        rknn_destroy(context);
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return result == RKNN_SUCC
                   ? Status{StatusCode::INVALID_ARGUMENT, "RKNN expects one input/output"}
                   : rknn_error("RKNN_QUERY_IN_OUT_NUM", result);
    }

    rknn_tensor_attr input{};
    input.index = 0;
    result = rknn_query(context, RKNN_QUERY_INPUT_ATTR, &input, sizeof(input));
    if (result != RKNN_SUCC || input.n_dims != 4) {
        rknn_destroy(context);
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return result == RKNN_SUCC
                   ? Status{StatusCode::INVALID_ARGUMENT, "RKNN input must be rank 4"}
                   : rknn_error("RKNN_QUERY_INPUT_ATTR", result);
    }
    rknn_tensor_attr output{};
    output.index = 0;
    result = rknn_query(context, RKNN_QUERY_OUTPUT_ATTR, &output, sizeof(output));
    if (result != RKNN_SUCC || output.n_elems == 0) {
        rknn_destroy(context);
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return result == RKNN_SUCC
                   ? Status{StatusCode::INVALID_ARGUMENT, "RKNN output empty"}
                   : rknn_error("RKNN_QUERY_OUTPUT_ATTR", result);
    }

    VisionBackendInfo info;
    info.model_name = config_.model_name;
    if (input.fmt == RKNN_TENSOR_NHWC) {
        info.input_height = input.dims[1];
        info.input_width = input.dims[2];
        info.input_channels = input.dims[3];
    } else if (input.fmt == RKNN_TENSOR_NCHW) {
        info.input_channels = input.dims[1];
        info.input_height = input.dims[2];
        info.input_width = input.dims[3];
    } else {
        rknn_destroy(context);
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return {StatusCode::INVALID_ARGUMENT, "RKNN input layout unsupported"};
    }
    if (info.input_width == 0 || info.input_height == 0 || info.input_channels != 3) {
        rknn_destroy(context);
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return {StatusCode::INVALID_ARGUMENT, "RKNN input shape unsupported"};
    }
    info.input_layout = "NHWC";
    info.input_type = "UINT8 RGB";
    info.output_shape = tensor_shape(output);
    info.model_load_ms = std::chrono::duration<double, std::milli>(
                             clock_type::now() - load_start).count();
    rknn_sdk_version versions{};
    if (rknn_query(context, RKNN_QUERY_SDK_VERSION, &versions, sizeof(versions)) == RKNN_SUCC) {
        info.runtime_version = versions.api_version;
        info.driver_version = versions.drv_version;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        context_ = context;
        output_elements_ = output.n_elems;
        info_ = std::move(info);
        state_ = ModelState::Ready;
    }
    return Status::Ok();
}

Status RknnVisionBackend::infer(const VisionTensor& input, VisionResult& result) {
    rknn_context context = 0;
    std::uint32_t output_elements = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Ready || context_ == 0)
            return {StatusCode::INVALID_STATE, "RKNN vision not ready"};
        if (input.width != info_.input_width || input.height != info_.input_height ||
            input.channels != info_.input_channels || input.layout != "NHWC" ||
            input.color_order != "RGB" ||
            input.data.size() != static_cast<std::size_t>(input.width) * input.height * 3)
            return {StatusCode::INVALID_ARGUMENT, "RKNN input tensor mismatch"};
        state_ = ModelState::Busy;
        context = static_cast<rknn_context>(context_);
        output_elements = output_elements_;
    }

    auto slot = scheduler_.try_acquire(ModelKind::Vision);
    if (!slot.ok()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Ready;
        return slot;
    }
    SchedulerRelease release{scheduler_};
    const auto inference_start = clock_type::now();
    rknn_input rknn_input_value{};
    rknn_input_value.index = 0;
    rknn_input_value.buf = const_cast<std::uint8_t*>(input.data.data());
    rknn_input_value.size = static_cast<std::uint32_t>(input.data.size());
    rknn_input_value.pass_through = 0;
    rknn_input_value.type = RKNN_TENSOR_UINT8;
    rknn_input_value.fmt = RKNN_TENSOR_NHWC;
    int code = rknn_inputs_set(context, 1, &rknn_input_value);
    if (code == RKNN_SUCC) code = rknn_run(context, nullptr);
    rknn_output output{};
    output.index = 0;
    output.want_float = 1;
    if (code == RKNN_SUCC) code = rknn_outputs_get(context, 1, &output, nullptr);
    if (code != RKNN_SUCC) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Error;
        return rknn_error("RKNN inference", code);
    }

    const auto* values = static_cast<const float*>(output.buf);
    const auto available = std::min<std::size_t>(output_elements,
                                                  output.size / sizeof(float));
    std::vector<std::size_t> indices(available);
    std::iota(indices.begin(), indices.end(), 0);
    const auto count = std::min(config_.top_k, indices.size());
    std::partial_sort(indices.begin(), indices.begin() + count, indices.end(),
                      [values](std::size_t left, std::size_t right) {
                          return values[left] > values[right];
                      });
    result.classifications.clear();
    result.classifications.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto class_id = indices[index];
        result.classifications.push_back({static_cast<std::int32_t>(class_id),
            "ImageNet class " + std::to_string(class_id), values[class_id]});
    }
    rknn_outputs_release(context, 1, &output);
    result.inference_ms = std::chrono::duration<double, std::milli>(
                              clock_type::now() - inference_start).count();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Error) state_ = ModelState::Ready;
    }
    return Status::Ok();
}

void RknnVisionBackend::unload() {
    rknn_context context = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (context_ != 0) context = static_cast<rknn_context>(context_);
        context_ = 0;
        output_elements_ = 0;
        state_ = ModelState::Unloaded;
    }
    if (context != 0) rknn_destroy(context);
}

ModelState RknnVisionBackend::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

VisionBackendInfo RknnVisionBackend::info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return info_;
}

}  // namespace cockpit::infer
