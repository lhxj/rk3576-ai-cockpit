#pragma once

#include "cockpit/protocol/message.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cockpit::infer {

enum class ModelState { Unloaded, Loading, Ready, Busy, Error };
enum class ModelKind { Language, Vision };

class IInferenceScheduler {
public:
    virtual ~IInferenceScheduler() = default;
    virtual protocol::Status try_acquire(ModelKind kind) = 0;
    virtual void release(ModelKind kind) = 0;
};

// One shared slot for future RKLLM and RKNN owners; no NPU preemption assumed.
class SerialInferenceScheduler final : public IInferenceScheduler {
public:
    protocol::Status try_acquire(ModelKind kind) override;
    void release(ModelKind kind) override;
private:
    std::mutex mutex_;
    bool occupied_{false};
    ModelKind owner_{ModelKind::Language};
};

struct LanguageRequest {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    std::uint64_t generation{0};
    std::string prompt;
};

struct LanguageEvent {
    protocol::RequestId request_id{0};
    protocol::SessionId session_id{0};
    std::uint64_t generation{0};
    std::string text;
    bool terminal{false};
    protocol::Status status;
};
using LanguageCallback = std::function<void(const LanguageEvent&)>;

class ILanguageModelBackend {
public:
    virtual ~ILanguageModelBackend() = default;
    virtual protocol::Status load() = 0;
    virtual void unload() = 0;
    virtual protocol::Status generate(LanguageRequest request, LanguageCallback callback) = 0;
    virtual protocol::Status cancel(protocol::SessionId session_id) = 0;
    virtual ModelState state() const = 0;
};

struct VisionFrame {
    std::string camera_id;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string pixel_format;
    std::uint32_t bytes_per_line{0};
    std::uint32_t bytes_used{0};
    std::uint64_t sequence{0};
    std::uint64_t stream_epoch{0};
    std::int64_t capture_timestamp_ns{0};
    std::int64_t dequeue_steady_timestamp_ns{0};
    std::shared_ptr<const std::vector<std::uint8_t>> payload;
};

struct LetterboxTransform {
    std::uint32_t source_width{0};
    std::uint32_t source_height{0};
    std::uint32_t target_width{0};
    std::uint32_t target_height{0};
    std::uint32_t resized_width{0};
    std::uint32_t resized_height{0};
    std::uint32_t pad_left{0};
    std::uint32_t pad_top{0};
    float scale{0.0F};
};

struct VisionTensor {
    VisionFrame frame;
    LetterboxTransform transform;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t channels{3};
    std::string color_order{"RGB"};
    std::string layout{"NHWC"};
    std::vector<std::uint8_t> data;
};

struct Classification {
    std::int32_t class_id{-1};
    std::string label;
    float confidence{0.0F};
};

// Coordinates use original source-image pixels, not model-input pixels.
struct Detection {
    std::int32_t class_id{-1};
    std::string label;
    float confidence{0.0F};
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct VisionResult {
    std::string camera_id;
    std::uint64_t frame_sequence{0};
    std::uint64_t stream_epoch{0};
    std::uint64_t inference_id{0};
    std::int64_t capture_timestamp_ns{0};
    std::int64_t inference_timestamp_ns{0};
    std::vector<Classification> classifications;
    std::vector<Detection> detections;
    double preprocess_ms{0.0};
    double inference_ms{0.0};
    double postprocess_ms{0.0};
    double end_to_end_ms{0.0};
};

struct VisionBackendInfo {
    std::string model_name;
    std::string runtime_version;
    std::string driver_version;
    std::uint32_t input_width{0};
    std::uint32_t input_height{0};
    std::uint32_t input_channels{0};
    std::string input_layout;
    std::string input_type;
    std::string output_shape;
    double model_load_ms{0.0};
};

class IVisionBackend {
public:
    virtual ~IVisionBackend() = default;
    virtual protocol::Status load() = 0;
    virtual protocol::Status infer(const VisionTensor& input, VisionResult& result) = 0;
    virtual void unload() = 0;
    virtual ModelState state() const = 0;
    virtual VisionBackendInfo info() const = 0;
};

// Host fixture with a joinable worker; callback must be quick and non-reentrant.
class MockLanguageModelBackend final : public ILanguageModelBackend {
public:
    MockLanguageModelBackend(IInferenceScheduler& scheduler,
                             std::vector<std::string> chunks,
                             std::chrono::milliseconds delay);
    ~MockLanguageModelBackend() override;
    protocol::Status load() override;
    void unload() override;
    protocol::Status generate(LanguageRequest request, LanguageCallback callback) override;
    protocol::Status cancel(protocol::SessionId session_id) override;
    ModelState state() const override;

private:
    void run(LanguageRequest request, LanguageCallback callback);
    IInferenceScheduler& scheduler_;
    const std::vector<std::string> chunks_;
    const std::chrono::milliseconds delay_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    ModelState state_{ModelState::Unloaded};
    protocol::SessionId active_session_{0};
    bool cancelled_{false};
    std::thread worker_;
};

}  // namespace cockpit::infer
