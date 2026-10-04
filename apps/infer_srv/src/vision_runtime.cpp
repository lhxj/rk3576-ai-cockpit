#include "cockpit/infer/vision_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cockpit::infer {
namespace {
using protocol::Status;
using protocol::StatusCode;
using clock_type = std::chrono::steady_clock;

std::int64_t steady_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               clock_type::now().time_since_epoch()).count();
}

std::uint8_t clamp_byte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

double elapsed_ms(clock_type::time_point start, clock_type::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
}  // namespace

Status Nv12Preprocessor::convert(const VisionFrame& frame, std::uint32_t target_width,
                                 std::uint32_t target_height, std::uint8_t pad_value,
                                 VisionTensor& output) {
    if (frame.pixel_format != "NV12" || !frame.payload || frame.width == 0 ||
        frame.height == 0 || target_width == 0 || target_height == 0 ||
        frame.width % 2 != 0 || frame.height % 2 != 0 ||
        frame.bytes_per_line < frame.width) {
        return {StatusCode::INVALID_ARGUMENT, "NV12 frame geometry"};
    }
    const auto required = static_cast<std::uint64_t>(frame.bytes_per_line) *
                          frame.height * 3U / 2U;
    if (required > frame.payload->size() || required > frame.bytes_used)
        return {StatusCode::INVALID_ARGUMENT, "NV12 payload/stride"};

    const float scale = std::min(static_cast<float>(target_width) / frame.width,
                                 static_cast<float>(target_height) / frame.height);
    const auto resized_width = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(frame.width * scale)));
    const auto resized_height = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(frame.height * scale)));
    const auto pad_left = (target_width - resized_width) / 2;
    const auto pad_top = (target_height - resized_height) / 2;

    output = {};
    output.frame = frame;
    output.width = target_width;
    output.height = target_height;
    output.channels = 3;
    output.color_order = "RGB";
    output.layout = "NHWC";
    output.transform = {frame.width, frame.height, target_width, target_height,
                        resized_width, resized_height, pad_left, pad_top, scale};
    output.data.assign(static_cast<std::size_t>(target_width) * target_height * 3,
                       pad_value);

    const auto* source = frame.payload->data();
    const auto uv_offset = static_cast<std::size_t>(frame.bytes_per_line) * frame.height;
    for (std::uint32_t dy = 0; dy < resized_height; ++dy) {
        const auto sy = std::min<std::uint32_t>(
            frame.height - 1, static_cast<std::uint32_t>(dy / scale));
        for (std::uint32_t dx = 0; dx < resized_width; ++dx) {
            const auto sx = std::min<std::uint32_t>(
                frame.width - 1, static_cast<std::uint32_t>(dx / scale));
            const int y = source[static_cast<std::size_t>(sy) * frame.bytes_per_line + sx];
            const auto uv_index = uv_offset + static_cast<std::size_t>(sy / 2) *
                                      frame.bytes_per_line + (sx & ~1U);
            const int u = source[uv_index] - 128;
            const int v = source[uv_index + 1] - 128;
            const int c = std::max(0, y - 16);
            const int r = (298 * c + 409 * v + 128) >> 8;
            const int g = (298 * c - 100 * u - 208 * v + 128) >> 8;
            const int b = (298 * c + 516 * u + 128) >> 8;
            const auto destination = (static_cast<std::size_t>(dy + pad_top) * target_width +
                                      dx + pad_left) * 3;
            output.data[destination] = clamp_byte(r);
            output.data[destination + 1] = clamp_byte(g);
            output.data[destination + 2] = clamp_byte(b);
        }
    }
    return Status::Ok();
}

Detection Nv12Preprocessor::map_detection_to_source(
    const Detection& model_detection, const LetterboxTransform& transform) {
    Detection mapped = model_detection;
    if (transform.scale <= 0.0F) return mapped;
    const float left = std::clamp(
        (model_detection.x - transform.pad_left) / transform.scale,
        0.0F, static_cast<float>(transform.source_width));
    const float top = std::clamp(
        (model_detection.y - transform.pad_top) / transform.scale,
        0.0F, static_cast<float>(transform.source_height));
    const float right = std::clamp(
        (model_detection.x + model_detection.width - transform.pad_left) /
            transform.scale,
        0.0F, static_cast<float>(transform.source_width));
    const float bottom = std::clamp(
        (model_detection.y + model_detection.height - transform.pad_top) /
            transform.scale,
        0.0F, static_cast<float>(transform.source_height));
    mapped.x = left;
    mapped.y = top;
    mapped.width = std::max(0.0F, right - left);
    mapped.height = std::max(0.0F, bottom - top);
    return mapped;
}

VisionRuntime::VisionRuntime(IVisionBackend& backend, VisionRuntimeConfig config)
    : backend_(backend), config_(std::move(config)) {}

VisionRuntime::~VisionRuntime() {
    if (running()) (void)stop();
}

Status VisionRuntime::start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return {StatusCode::INVALID_STATE, "vision runtime active"};
        if (config_.queue_capacity == 0 || config_.target_fps <= 0.0 ||
            !std::isfinite(config_.target_fps))
            return {StatusCode::INVALID_ARGUMENT, "vision runtime configuration"};
        snapshot_ = {};
        snapshot_.state = VisionRuntimeState::LOADING;
        queue_.clear();
        current_epoch_ = 0;
        last_sample_timestamp_ns_ = 0;
        first_result_timestamp_ns_ = 0;
        last_result_timestamp_ns_ = 0;
        preprocess_total_ms_ = 0.0;
        inference_total_ms_ = 0.0;
        postprocess_total_ms_ = 0.0;
        end_to_end_total_ms_ = 0.0;
        stopping_ = false;
    }
    notify_update();
    auto status = backend_.load();
    if (!status.ok()) {
        set_state(VisionRuntimeState::ERROR);
        return status;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.backend = backend_.info();
        snapshot_.state = VisionRuntimeState::READY;
        running_ = true;
        accepting_ = true;
        try {
            worker_ = std::thread(&VisionRuntime::run, this);
        } catch (...) {
            running_ = false;
            accepting_ = false;
            snapshot_.state = VisionRuntimeState::ERROR;
            backend_.unload();
            return {StatusCode::INTERNAL_ERROR, "vision worker start"};
        }
    }
    notify_update();
    return Status::Ok();
}

Status VisionRuntime::submit(VisionFrame frame) {
    if (!frame.payload || frame.camera_id.empty() || frame.stream_epoch == 0)
        return {StatusCode::INVALID_ARGUMENT, "vision frame metadata"};
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!accepting_ || stopping_)
            return {StatusCode::INVALID_STATE, "vision runtime stopped"};
        auto& metrics = snapshot_.metrics;
        ++metrics.vision_input_frames;
        if (current_epoch_ != 0 && frame.stream_epoch < current_epoch_) {
            ++metrics.stale_epoch_drop_count;
            ++metrics.vision_drop_count;
            return {StatusCode::STALE_EPOCH, "old vision input epoch"};
        }
        if (frame.stream_epoch > current_epoch_) {
            current_epoch_ = frame.stream_epoch;
            last_sample_timestamp_ns_ = 0;
            metrics.queue_drop_count += queue_.size();
            metrics.vision_drop_count += queue_.size();
            queue_.clear();
        }
        const auto timestamp = frame.dequeue_steady_timestamp_ns > 0
                                   ? frame.dequeue_steady_timestamp_ns : steady_ns();
        const auto interval = static_cast<std::int64_t>(
            1'000'000'000.0 / config_.target_fps);
        if (last_sample_timestamp_ns_ > 0 &&
            timestamp - last_sample_timestamp_ns_ < interval) {
            ++metrics.sampling_drop_count;
            ++metrics.vision_drop_count;
            return Status::Ok();
        }
        last_sample_timestamp_ns_ = timestamp;
        if (queue_.size() >= config_.queue_capacity) {
            queue_.pop_front();
            ++metrics.queue_drop_count;
            ++metrics.vision_drop_count;
        }
        queue_.push_back(std::move(frame));
        metrics.queue_peak = std::max(metrics.queue_peak, queue_.size());
        wake = true;
    }
    if (wake) ready_.notify_one();
    return Status::Ok();
}

Status VisionRuntime::stop() {
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && !worker_.joinable())
            return {StatusCode::INVALID_STATE, "vision runtime inactive"};
        accepting_ = false;
        stopping_ = true;
        snapshot_.metrics.queue_drop_count += queue_.size();
        snapshot_.metrics.vision_drop_count += queue_.size();
        queue_.clear();
        worker = std::move(worker_);
    }
    ready_.notify_all();
    if (worker.joinable()) worker.join();
    backend_.unload();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        stopping_ = false;
        snapshot_.state = VisionRuntimeState::NOT_READY;
    }
    notify_update();
    return Status::Ok();
}

void VisionRuntime::set_update_callback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    update_callback_ = std::move(callback);
}

VisionRuntimeSnapshot VisionRuntime::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool VisionRuntime::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void VisionRuntime::run() {
    while (true) {
        VisionFrame frame;
        VisionBackendInfo backend_info;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) break;
            frame = std::move(queue_.front());
            queue_.pop_front();
            backend_info = snapshot_.backend;
        }

        const auto start = clock_type::now();
        VisionTensor tensor;
        auto status = Nv12Preprocessor::convert(
            frame, backend_info.input_width, backend_info.input_height,
            config_.letterbox_value, tensor);
        const auto after_preprocess = clock_type::now();
        VisionResult result;
        if (status.ok()) status = backend_.infer(tensor, result);
        const auto after_inference = clock_type::now();
        if (!status.ok()) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++snapshot_.metrics.inference_error_count;
                ++snapshot_.metrics.vision_drop_count;
                if (status.code != StatusCode::UNAVAILABLE)
                    snapshot_.state = VisionRuntimeState::ERROR;
            }
            notify_update();
            continue;
        }

        result.camera_id = frame.camera_id;
        result.frame_sequence = frame.sequence;
        result.stream_epoch = frame.stream_epoch;
        result.inference_id = next_inference_id_.fetch_add(1);
        result.capture_timestamp_ns = frame.capture_timestamp_ns;
        result.inference_timestamp_ns = steady_ns();
        result.preprocess_ms = elapsed_ms(start, after_preprocess);
        if (result.inference_ms <= 0.0)
            result.inference_ms = elapsed_ms(after_preprocess, after_inference);
        for (auto& detection : result.detections)
            detection = Nv12Preprocessor::map_detection_to_source(detection,
                                                                  tensor.transform);
        const auto after_postprocess = clock_type::now();
        result.postprocess_ms += elapsed_ms(after_inference, after_postprocess);
        if (frame.dequeue_steady_timestamp_ns > 0 &&
            result.inference_timestamp_ns >= frame.dequeue_steady_timestamp_ns) {
            result.end_to_end_ms = static_cast<double>(result.inference_timestamp_ns -
                                                       frame.dequeue_steady_timestamp_ns) /
                                   1'000'000.0;
        } else {
            result.end_to_end_ms = elapsed_ms(start, after_postprocess);
        }

        bool publish = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& metrics = snapshot_.metrics;
            if (frame.stream_epoch != current_epoch_) {
                ++metrics.stale_epoch_drop_count;
                ++metrics.vision_drop_count;
            } else if (!stopping_) {
                ++metrics.vision_inferred_frames;
                preprocess_total_ms_ += result.preprocess_ms;
                inference_total_ms_ += result.inference_ms;
                postprocess_total_ms_ += result.postprocess_ms;
                end_to_end_total_ms_ += result.end_to_end_ms;
                const auto count = static_cast<double>(metrics.vision_inferred_frames);
                metrics.preprocess_ms_average = preprocess_total_ms_ / count;
                metrics.inference_ms_average = inference_total_ms_ / count;
                metrics.postprocess_ms_average = postprocess_total_ms_ / count;
                metrics.end_to_end_ms_average = end_to_end_total_ms_ / count;
                last_result_timestamp_ns_ = result.inference_timestamp_ns;
                if (first_result_timestamp_ns_ == 0)
                    first_result_timestamp_ns_ = last_result_timestamp_ns_;
                const auto elapsed = last_result_timestamp_ns_ - first_result_timestamp_ns_;
                if (elapsed > 0 && metrics.vision_inferred_frames > 1)
                    metrics.vision_fps = static_cast<double>(metrics.vision_inferred_frames - 1) *
                                         1'000'000'000.0 / elapsed;
                snapshot_.latest_result = std::move(result);
                snapshot_.state = VisionRuntimeState::RUNNING;
                publish = true;
            }
        }
        if (publish) notify_update();
    }
}

void VisionRuntime::notify_update() {
    UpdateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = update_callback_;
    }
    if (callback) {
        try { callback(); } catch (...) {}
    }
}

void VisionRuntime::set_state(VisionRuntimeState state) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.state = state;
    }
    notify_update();
}

FakeVisionBackend::FakeVisionBackend(IInferenceScheduler& scheduler,
                                     std::chrono::milliseconds delay)
    : scheduler_(scheduler), delay_(delay) {}

Status FakeVisionBackend::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != ModelState::Unloaded)
        return {StatusCode::INVALID_STATE, "fake vision already loaded"};
    state_ = ModelState::Ready;
    ++load_count_;
    return Status::Ok();
}

Status FakeVisionBackend::infer(const VisionTensor& input, VisionResult& result) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModelState::Ready)
            return {StatusCode::INVALID_STATE, "fake vision not ready"};
        state_ = ModelState::Busy;
    }
    auto slot = scheduler_.try_acquire(ModelKind::Vision);
    if (!slot.ok()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ModelState::Ready;
        return slot;
    }
    const auto start = clock_type::now();
    if (delay_.count() > 0) std::this_thread::sleep_for(delay_);
    result.classifications = {{static_cast<std::int32_t>(input.frame.sequence % 1001),
                               "fixture", 0.75F}};
    result.inference_ms = elapsed_ms(start, clock_type::now());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++infer_count_;
        state_ = ModelState::Ready;
    }
    scheduler_.release(ModelKind::Vision);
    return Status::Ok();
}

void FakeVisionBackend::unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ModelState::Unloaded;
}

ModelState FakeVisionBackend::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

VisionBackendInfo FakeVisionBackend::info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return info_;
}

std::uint64_t FakeVisionBackend::load_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return load_count_;
}

std::uint64_t FakeVisionBackend::infer_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return infer_count_;
}

const char* vision_runtime_state_name(VisionRuntimeState state) {
    switch (state) {
        case VisionRuntimeState::NOT_READY: return "NOT_READY";
        case VisionRuntimeState::LOADING: return "LOADING";
        case VisionRuntimeState::READY: return "READY";
        case VisionRuntimeState::RUNNING: return "RUNNING";
        case VisionRuntimeState::ERROR: return "ERROR";
    }
    return "ERROR";
}

}  // namespace cockpit::infer
