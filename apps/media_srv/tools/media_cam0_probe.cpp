#include "cockpit/media/media_service.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
using cockpit::media::CameraCaptureConfig;
using cockpit::media::CaptureStats;
using cockpit::media::ICameraCapture;
using cockpit::media::MediaOperation;
using cockpit::media::MediaOperationResult;
using cockpit::media::MediaService;
using cockpit::media::MediaServiceConfig;
using cockpit::media::MediaStatus;
using cockpit::media::V4l2MplaneCameraCapture;

struct Options {
    std::string mode;
    std::string device;
    std::string snapshot_directory{"/home/cat/cockpit/snapshots"};
    std::uint64_t frames{300};
    std::uint32_t cycles{20};
    std::uint64_t frames_per_cycle{10};
    std::uint32_t width{1632};
    std::uint32_t height{1224};
    std::string pixel_format{"NV12"};
};

std::uint64_t parse_unsigned(const std::string& text, const char* name) {
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed, 10);
    if (consumed != text.size() || value == 0) throw std::runtime_error(std::string("invalid ") + name);
    return value;
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        const auto take = [&](const char* name) -> std::string {
            if (++index >= argc) throw std::runtime_error(std::string("missing value for ") + name);
            return argv[index];
        };
        if (argument == "--mode") options.mode = take("--mode");
        else if (argument == "--device") options.device = take("--device");
        else if (argument == "--snapshot-dir") options.snapshot_directory = take("--snapshot-dir");
        else if (argument == "--frames") options.frames = parse_unsigned(take("--frames"), "frames");
        else if (argument == "--cycles")
            options.cycles = static_cast<std::uint32_t>(parse_unsigned(take("--cycles"), "cycles"));
        else if (argument == "--frames-per-cycle")
            options.frames_per_cycle = parse_unsigned(take("--frames-per-cycle"), "frames-per-cycle");
        else if (argument == "--width")
            options.width = static_cast<std::uint32_t>(parse_unsigned(take("--width"), "width"));
        else if (argument == "--height")
            options.height = static_cast<std::uint32_t>(parse_unsigned(take("--height"), "height"));
        else if (argument == "--pixel-format") options.pixel_format = take("--pixel-format");
        else throw std::runtime_error("unknown argument: " + argument);
    }
    if (options.mode != "negotiate" && options.mode != "capture" &&
        options.mode != "restart" && options.mode != "snapshot" && options.mode != "core")
        throw std::runtime_error("--mode must be negotiate|capture|restart|snapshot|core");
    if (options.device.empty()) throw std::runtime_error("--device is required");
    return options;
}

CameraCaptureConfig capture_config(const Options& options) {
    CameraCaptureConfig config;
    config.device = options.device;
    config.camera_id = "front";
    config.width = options.width;
    config.height = options.height;
    config.pixel_format = options.pixel_format;
    config.fps = 30;
    config.buffer_count = 4;
    config.poll_timeout_ms = 200;
    return config;
}

void require(const MediaStatus& status, const char* operation) {
    if (!status.ok()) throw std::runtime_error(std::string(operation) + ": " + status.detail);
}

void print_format(const cockpit::media::CameraFormat& format) {
    std::cout << "driver=" << format.driver << '\n'
              << "width=" << format.width << '\n'
              << "height=" << format.height << '\n'
              << "pixel_format=" << format.pixel_format << '\n'
              << "num_planes=" << format.num_planes << '\n'
              << "bytes_per_line=" << format.bytes_per_line << '\n'
              << "size_image=" << format.size_image << '\n'
              << "requested_buffers=" << format.requested_buffers << '\n'
              << "actual_buffers=" << format.actual_buffers << '\n'
              << "frame_interval_supported=" << (format.frame_interval_supported ? "true" : "false") << '\n';
    if (format.frame_interval_supported)
        std::cout << "time_per_frame=" << format.fps_numerator << '/' << format.fps_denominator << '\n';
    else
        std::cout << "time_per_frame=UNSUPPORTED_BY_DRIVER\n";
}

void print_stats(const CaptureStats& stats) {
    const auto elapsed_ns = stats.last_dequeue_steady_ns - stats.first_dequeue_steady_ns;
    const double elapsed = elapsed_ns > 0 ? static_cast<double>(elapsed_ns) / 1'000'000'000.0 : 0.0;
    const double fps = elapsed > 0.0 && stats.frames > 1
                           ? static_cast<double>(stats.frames - 1) / elapsed
                           : 0.0;
    std::cout << "stream_epoch=" << stats.stream_epoch << '\n'
              << "frames=" << stats.frames << '\n'
              << "elapsed_seconds=" << elapsed << '\n'
              << "capture_fps=" << fps << '\n'
              << "sequence_gaps=" << stats.sequence_gap_count << '\n'
              << "poll_timeouts=" << stats.poll_timeouts << '\n'
              << "dqbuf_errors=" << stats.dequeue_errors << '\n'
              << "qbuf_errors=" << stats.queue_errors << '\n'
              << "bytes_used_min=" << (stats.frames == 0 ? 0 : stats.bytes_used_min) << '\n'
              << "bytes_used_max=" << stats.bytes_used_max << '\n';
}

class FrameWaiter {
public:
    void receive(cockpit::media::CapturedFrame frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        last_sequence_ = frame.sequence;
        last_epoch_ = frame.stream_epoch;
        ready_.notify_all();
    }

    bool wait_for(std::uint64_t target, std::chrono::seconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, timeout, [&] { return count_ >= target; });
    }

    [[nodiscard]] std::uint64_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }
    [[nodiscard]] std::uint64_t last_sequence() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_sequence_;
    }
    [[nodiscard]] std::uint64_t last_epoch() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_epoch_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::uint64_t count_{0};
    std::uint64_t last_sequence_{0};
    std::uint64_t last_epoch_{0};
};

std::size_t directory_entries(const char* path) {
    std::error_code error;
    std::size_t count = 0;
    for (std::filesystem::directory_iterator it(path, error), end; !error && it != end; it.increment(error)) ++count;
    return error ? 0 : count;
}

int negotiate(const Options& options) {
    V4l2MplaneCameraCapture capture;
    require(capture.open_device(options.device), "open");
    require(capture.configure(capture_config(options)), "configure");
    std::cout << "device=" << options.device << '\n';
    print_format(capture.actual_format());
    capture.close_device();
    std::cout << "close=OK\n";
    return 0;
}

CaptureStats capture_frames(V4l2MplaneCameraCapture& capture, const Options& options,
                            std::uint64_t frame_target) {
    require(capture.open_device(options.device), "open");
    require(capture.configure(capture_config(options)), "configure");
    FrameWaiter waiter;
    require(capture.start([&waiter](cockpit::media::CapturedFrame frame) {
        waiter.receive(std::move(frame));
    }), "start");
    const auto seconds = std::chrono::seconds(std::max<std::uint64_t>(10, frame_target / 10));
    if (!waiter.wait_for(frame_target, seconds)) {
        (void)capture.stop();
        capture.close_device();
        throw std::runtime_error("frame wait timeout at count=" + std::to_string(waiter.count()));
    }
    const auto status = capture.stop();
    const auto stats = capture.stats();
    require(status, "stop");
    std::cout << "callback_last_epoch=" << waiter.last_epoch() << '\n'
              << "callback_last_sequence=" << waiter.last_sequence() << '\n';
    capture.close_device();
    return stats;
}

int capture_mode(const Options& options) {
    V4l2MplaneCameraCapture capture;
    const auto stats = capture_frames(capture, options, options.frames);
    print_stats(stats);
    return stats.frames >= options.frames && stats.dequeue_errors == 0 && stats.queue_errors == 0 ? 0 : 2;
}

int restart_mode(const Options& options) {
    V4l2MplaneCameraCapture capture;
    const auto initial_fds = directory_entries("/proc/self/fd");
    const auto initial_threads = directory_entries("/proc/self/task");
    std::uint64_t previous_epoch = 0;
    for (std::uint32_t cycle = 1; cycle <= options.cycles; ++cycle) {
        const auto stats = capture_frames(capture, options, options.frames_per_cycle);
        if (stats.stream_epoch <= previous_epoch || stats.frames < options.frames_per_cycle ||
            stats.dequeue_errors != 0 || stats.queue_errors != 0)
            throw std::runtime_error("restart cycle validation failed");
        previous_epoch = stats.stream_epoch;
        std::cout << "cycle=" << cycle << " epoch=" << stats.stream_epoch
                  << " frames=" << stats.frames << '\n';
    }
    const auto final_fds = directory_entries("/proc/self/fd");
    const auto final_threads = directory_entries("/proc/self/task");
    std::cout << "cycles_completed=" << options.cycles << '\n'
              << "initial_fd_count=" << initial_fds << '\n'
              << "final_fd_count=" << final_fds << '\n'
              << "initial_thread_count=" << initial_threads << '\n'
              << "final_thread_count=" << final_threads << '\n';
    return initial_fds == final_fds && initial_threads == final_threads ? 0 : 2;
}

MediaOperationResult submit_and_wait(MediaService& service, MediaOperation operation,
                                     std::chrono::seconds timeout = 5s) {
    auto promise = std::make_shared<std::promise<MediaOperationResult>>();
    auto future = promise->get_future();
    require(service.submit(operation, [promise](MediaOperationResult result) {
        promise->set_value(std::move(result));
    }), "submit");
    if (future.wait_for(timeout) != std::future_status::ready)
        throw std::runtime_error("media operation timeout");
    return future.get();
}

std::shared_ptr<MediaService> make_service(const Options& options) {
    MediaServiceConfig config;
    config.capture = capture_config(options);
    config.snapshot_directory = options.snapshot_directory;
    config.snapshot_wait = 2s;
    return std::make_shared<MediaService>(std::move(config),
                                          std::make_unique<V4l2MplaneCameraCapture>());
}

int snapshot_mode(const Options& options) {
    auto service = make_service(options);
    require(service->start(), "service start");
    const auto start = submit_and_wait(*service, MediaOperation::PreviewStart);
    require(start.status, "preview start");
    const auto snapshot = submit_and_wait(*service, MediaOperation::Snapshot);
    require(snapshot.status, "snapshot");
    const auto stop = submit_and_wait(*service, MediaOperation::PreviewStop);
    require(stop.status, "preview stop");
    const auto stats = service->capture_stats();
    service->stop();
    std::cout << "preview_start=" << start.status.detail << '\n'
              << "snapshot=" << snapshot.status.detail << '\n'
              << "snapshot_path=" << snapshot.output_path << '\n'
              << "snapshot_size=" << std::filesystem::file_size(snapshot.output_path) << '\n'
              << "preview_stop=" << stop.status.detail << '\n';
    print_stats(stats);
    return 0;
}

cockpit::vehicle::VehicleCommand command(cockpit::vehicle::VehicleCore& core,
                                         cockpit::vehicle::IClock& clock,
                                         cockpit::protocol::RequestId request_id,
                                         cockpit::vehicle::CommandType type,
                                         cockpit::vehicle::CommandParameters parameters = {}) {
    cockpit::vehicle::VehicleCommand value;
    value.request_id = request_id;
    value.boot_epoch = core.boot_epoch();
    value.deadline_ms = clock.now_ms() + 5000;
    value.source = cockpit::vehicle::CommandSource::TEST;
    value.command_type = type;
    value.parameters = std::move(parameters);
    return value;
}

void run_core_command(cockpit::vehicle::InProcessVehicleCoreClient& client,
                      const cockpit::vehicle::VehicleCommand& value, const char* name) {
    const auto submission = client.send_command(value);
    if (!submission.accepted())
        throw std::runtime_error(std::string(name) + " rejected: " + submission.status.detail);
    std::cout << name << "_ack_status=" << static_cast<int>(submission.ack.status.code)
              << " ack_lifecycle=" << submission.ack.lifecycle_sequence << '\n';
    if (submission.result.wait_for(8s) != std::future_status::ready)
        throw std::runtime_error(std::string(name) + " result timeout");
    const auto result = submission.result.get();
    std::cout << name << "_result_status=" << static_cast<int>(result.status.code)
              << " result_lifecycle=" << result.lifecycle_sequence
              << " detail=" << result.status.detail << '\n';
    if (!result.status.ok()) throw std::runtime_error(std::string(name) + " result failed");
}

int core_mode(const Options& options) {
    auto service = make_service(options);
    require(service->start(), "service start");
    auto media = std::make_shared<cockpit::media::RealMediaServiceAdapter>(service);
    auto voice = std::make_shared<cockpit::vehicle::MockVoiceAdapter>();
    auto rtos = std::make_shared<cockpit::vehicle::MockRtosAdapter>();
    auto system = std::make_shared<cockpit::vehicle::MockSystemAdapter>();
    auto registry = std::make_shared<cockpit::vehicle::ServiceRegistry>();
    registry->set(cockpit::vehicle::ServiceDomain::MEDIA,
                  cockpit::vehicle::ServiceHealth::ONLINE,
                  cockpit::vehicle::StateSource::RUNTIME);
    auto clock = std::make_shared<cockpit::vehicle::SystemClock>();
    cockpit::vehicle::VehicleCore core({20261001, 16, 16, 64, 5ms},
                                       {media, voice, rtos, system}, registry, clock);
    const auto started = core.start();
    if (!started.ok()) throw std::runtime_error("core start: " + started.detail);
    cockpit::vehicle::InProcessVehicleCoreClient client(core);
    std::uint64_t request_id = 1;
    run_core_command(client, command(core, *clock, request_id++,
                                     cockpit::vehicle::CommandType::CAMERA_SELECT,
                                     {{"camera", "front"}}), "camera_select");
    run_core_command(client, command(core, *clock, request_id++,
                                     cockpit::vehicle::CommandType::CAMERA_PREVIEW_START),
                     "preview_start");
    auto state = client.get_snapshot();
    std::cout << "preview_start_revision=" << state.revision
              << " preview_state=" << static_cast<int>(state.preview.value)
              << " preview_source=" << static_cast<int>(state.preview.source) << '\n';
    run_core_command(client, command(core, *clock, request_id++,
                                     cockpit::vehicle::CommandType::CAMERA_SNAPSHOT),
                     "snapshot");
    run_core_command(client, command(core, *clock, request_id++,
                                     cockpit::vehicle::CommandType::CAMERA_PREVIEW_STOP),
                     "preview_stop");
    state = client.get_snapshot();
    std::cout << "preview_stop_revision=" << state.revision
              << " preview_state=" << static_cast<int>(state.preview.value)
              << " preview_source=" << static_cast<int>(state.preview.source) << '\n';
    core.stop();
    service->stop();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        std::cout << "mode=" << options.mode << '\n' << "device=" << options.device << '\n';
        if (options.mode == "negotiate") return negotiate(options);
        if (options.mode == "capture") return capture_mode(options);
        if (options.mode == "restart") return restart_mode(options);
        if (options.mode == "snapshot") return snapshot_mode(options);
        return core_mode(options);
    } catch (const std::exception& error) {
        std::cerr << "media_cam0_probe: FAIL: " << error.what() << '\n';
        return 1;
    }
}
