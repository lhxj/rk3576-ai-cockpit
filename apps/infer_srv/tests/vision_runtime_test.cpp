#include "cockpit/infer/vision_runtime.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

#define CHECK(expression)                                                                  \
    do {                                                                                   \
        if (!(expression)) {                                                               \
            std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #expression << '\n'; \
            return 1;                                                                      \
        }                                                                                  \
    } while (false)

namespace {

cockpit::infer::VisionFrame frame(std::uint64_t sequence, std::uint64_t epoch,
                                  std::int64_t timestamp_ns) {
    cockpit::infer::VisionFrame value;
    value.camera_id = "front";
    value.width = 4;
    value.height = 2;
    value.pixel_format = "NV12";
    value.bytes_per_line = 6;
    value.bytes_used = 18;
    value.sequence = sequence;
    value.stream_epoch = epoch;
    value.capture_timestamp_ns = timestamp_ns;
    value.dequeue_steady_timestamp_ns = timestamp_ns;
    auto payload = std::make_shared<std::vector<std::uint8_t>>(18, 128);
    for (std::size_t index = 0; index < 12; ++index) (*payload)[index] = 82;
    value.payload = std::move(payload);
    return value;
}

bool wait_for_inferred(cockpit::infer::VisionRuntime& runtime, std::uint64_t count,
                       std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (runtime.snapshot().metrics.vision_inferred_frames >= count) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

}  // namespace

int main() {
    using namespace cockpit::infer;
    using namespace std::chrono_literals;

    VisionTensor tensor;
    CHECK(Nv12Preprocessor::convert(frame(1, 1, 1), 4, 4, 114, tensor).ok());
    CHECK(tensor.width == 4 && tensor.height == 4 && tensor.data.size() == 48);
    CHECK(tensor.transform.resized_width == 4 && tensor.transform.resized_height == 2);
    CHECK(tensor.transform.pad_top == 1 && tensor.transform.pad_left == 0);
    CHECK(tensor.data[0] == 114 && tensor.data[1] == 114 && tensor.data[2] == 114);
    CHECK(tensor.data[12] != 114);
    Detection model_box{1, "box", 0.9F, 1.0F, 1.0F, 2.0F, 2.0F};
    const auto source_box = Nv12Preprocessor::map_detection_to_source(
        model_box, tensor.transform);
    CHECK(std::abs(source_box.x - 1.0F) < 0.01F);
    CHECK(std::abs(source_box.y - 0.0F) < 0.01F);

    SerialInferenceScheduler scheduler;
    FakeVisionBackend slow_backend(scheduler, 15ms);
    VisionRuntimeConfig queue_config;
    queue_config.target_fps = 1000.0;
    queue_config.queue_capacity = 2;
    VisionRuntime runtime(slow_backend, queue_config);
    CHECK(runtime.start().ok());
    for (std::uint64_t sequence = 1; sequence <= 40; ++sequence)
        CHECK(runtime.submit(frame(sequence, 1, static_cast<std::int64_t>(sequence) *
                                                 2'000'000)).ok());
    CHECK(wait_for_inferred(runtime, 1));
    std::this_thread::sleep_for(80ms);
    const auto bounded = runtime.snapshot();
    CHECK(bounded.metrics.queue_peak <= 2);
    CHECK(bounded.metrics.queue_drop_count > 0);
    CHECK(bounded.metrics.vision_drop_count > 0);
    CHECK(bounded.latest_result.has_value());
    CHECK(bounded.latest_result->frame_sequence == 40);
    CHECK(runtime.stop().ok());

    FakeVisionBackend epoch_backend(scheduler, 30ms);
    VisionRuntime epoch_runtime(epoch_backend, queue_config);
    CHECK(epoch_runtime.start().ok());
    CHECK(epoch_runtime.submit(frame(10, 5, 10'000'000)).ok());
    std::this_thread::sleep_for(5ms);
    CHECK(epoch_runtime.submit(frame(1, 6, 20'000'000)).ok());
    CHECK(wait_for_inferred(epoch_runtime, 1));
    const auto epoch = epoch_runtime.snapshot();
    CHECK(epoch.metrics.stale_epoch_drop_count >= 1);
    CHECK(epoch.latest_result && epoch.latest_result->stream_epoch == 6);
    CHECK(epoch.latest_result->frame_sequence == 1);
    CHECK(epoch_runtime.stop().ok());

    FakeVisionBackend restart_backend(scheduler);
    VisionRuntime restart_runtime(restart_backend, VisionRuntimeConfig{30.0, 1, 114});
    for (int iteration = 0; iteration < 100; ++iteration) {
        CHECK(restart_runtime.start().ok());
        CHECK(restart_runtime.submit(frame(1, static_cast<std::uint64_t>(iteration + 1),
                                           1'000'000)).ok());
        CHECK(wait_for_inferred(restart_runtime, 1));
        CHECK(restart_runtime.stop().ok());
    }
    CHECK(restart_backend.load_count() == 100);

    std::cout << "vision_runtime_test: PASS\n";
    return 0;
}
