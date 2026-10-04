#pragma once

#include "cockpit/infer/infer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace cockpit::infer {

enum class VisionRuntimeState { NOT_READY, LOADING, READY, RUNNING, ERROR };

struct VisionRuntimeConfig {
    double target_fps{8.0};
    std::size_t queue_capacity{2};
    std::uint8_t letterbox_value{114};
};

struct VisionRuntimeMetrics {
    std::uint64_t vision_input_frames{0};
    std::uint64_t vision_inferred_frames{0};
    std::uint64_t vision_drop_count{0};
    std::uint64_t sampling_drop_count{0};
    std::uint64_t queue_drop_count{0};
    std::uint64_t stale_epoch_drop_count{0};
    std::uint64_t inference_error_count{0};
    std::size_t queue_peak{0};
    double vision_fps{0.0};
    double preprocess_ms_average{0.0};
    double inference_ms_average{0.0};
    double postprocess_ms_average{0.0};
    double end_to_end_ms_average{0.0};
};

struct VisionRuntimeSnapshot {
    VisionRuntimeState state{VisionRuntimeState::NOT_READY};
    VisionBackendInfo backend;
    VisionRuntimeMetrics metrics;
    std::optional<VisionResult> latest_result;
};

class Nv12Preprocessor {
public:
    static protocol::Status convert(const VisionFrame& frame, std::uint32_t target_width,
                                    std::uint32_t target_height, std::uint8_t pad_value,
                                    VisionTensor& output);
    static Detection map_detection_to_source(const Detection& model_detection,
                                              const LetterboxTransform& transform);
};

class VisionRuntime final {
public:
    using UpdateCallback = std::function<void()>;

    VisionRuntime(IVisionBackend& backend, VisionRuntimeConfig config = {});
    ~VisionRuntime();
    VisionRuntime(const VisionRuntime&) = delete;
    VisionRuntime& operator=(const VisionRuntime&) = delete;

    protocol::Status start();
    protocol::Status submit(VisionFrame frame);
    protocol::Status stop();
    void set_update_callback(UpdateCallback callback);
    [[nodiscard]] VisionRuntimeSnapshot snapshot() const;
    [[nodiscard]] bool running() const;

private:
    void run();
    void notify_update();
    void set_state(VisionRuntimeState state);

    IVisionBackend& backend_;
    const VisionRuntimeConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<VisionFrame> queue_;
    std::thread worker_;
    VisionRuntimeSnapshot snapshot_;
    UpdateCallback update_callback_;
    bool running_{false};
    bool accepting_{false};
    bool stopping_{false};
    std::uint64_t current_epoch_{0};
    std::int64_t last_sample_timestamp_ns_{0};
    std::int64_t first_result_timestamp_ns_{0};
    std::int64_t last_result_timestamp_ns_{0};
    double preprocess_total_ms_{0.0};
    double inference_total_ms_{0.0};
    double postprocess_total_ms_{0.0};
    double end_to_end_total_ms_{0.0};
    std::atomic<std::uint64_t> next_inference_id_{1};
};

class FakeVisionBackend final : public IVisionBackend {
public:
    explicit FakeVisionBackend(IInferenceScheduler& scheduler,
                               std::chrono::milliseconds delay = {});
    protocol::Status load() override;
    protocol::Status infer(const VisionTensor& input, VisionResult& result) override;
    void unload() override;
    ModelState state() const override;
    VisionBackendInfo info() const override;
    [[nodiscard]] std::uint64_t load_count() const;
    [[nodiscard]] std::uint64_t infer_count() const;

private:
    IInferenceScheduler& scheduler_;
    const std::chrono::milliseconds delay_;
    mutable std::mutex mutex_;
    ModelState state_{ModelState::Unloaded};
    VisionBackendInfo info_{"FakeVision", "host", "host", 224, 224, 3,
                            "NHWC", "UINT8", "1x3", 0.0};
    std::uint64_t load_count_{0};
    std::uint64_t infer_count_{0};
};

const char* vision_runtime_state_name(VisionRuntimeState state);

}  // namespace cockpit::infer
