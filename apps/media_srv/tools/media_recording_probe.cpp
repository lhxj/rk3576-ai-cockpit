#include "cockpit/media/media_service.hpp"
#include "cockpit/media/mpp_h264_recorder.hpp"
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
namespace media = cockpit::media;

struct Options {
    std::string mode;
    std::string device;
    std::string output_directory{"/home/cat/cockpit/recordings"};
    std::uint32_t seconds{10};
    std::uint32_t cycles{20};
    std::uint32_t width{1632};
    std::uint32_t height{1224};
};

std::uint32_t parse_positive(const std::string& value, const char* name) {
    std::size_t used = 0;
    const auto parsed = std::stoul(value, &used, 10);
    if (used != value.size() || parsed == 0)
        throw std::runtime_error(std::string("invalid ") + name);
    return static_cast<std::uint32_t>(parsed);
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        const auto take = [&](const char* name) {
            if (++index >= argc) throw std::runtime_error(std::string("missing ") + name);
            return std::string(argv[index]);
        };
        if (argument == "--mode") options.mode = take("--mode");
        else if (argument == "--device") options.device = take("--device");
        else if (argument == "--output-dir") options.output_directory = take("--output-dir");
        else if (argument == "--seconds")
            options.seconds = parse_positive(take("--seconds"), "seconds");
        else if (argument == "--cycles")
            options.cycles = parse_positive(take("--cycles"), "cycles");
        else if (argument == "--width")
            options.width = parse_positive(take("--width"), "width");
        else if (argument == "--height")
            options.height = parse_positive(take("--height"), "height");
        else throw std::runtime_error("unknown argument: " + argument);
    }
    if (options.mode != "synthetic" && options.mode != "record-only" &&
        options.mode != "shared" && options.mode != "restart")
        throw std::runtime_error("--mode must be synthetic|record-only|shared|restart");
    if (options.mode != "synthetic" && options.device.empty())
        throw std::runtime_error("--device is required for real capture");
    return options;
}

void require(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error(detail);
}

media::MediaOperationResult submit_and_wait(media::MediaService& service,
                                            media::MediaOperation operation,
                                            std::chrono::seconds timeout = 8s) {
    auto promise = std::make_shared<std::promise<media::MediaOperationResult>>();
    auto future = promise->get_future();
    const auto accepted = service.submit(operation, [promise](media::MediaOperationResult result) {
        promise->set_value(std::move(result));
    });
    if (!accepted.ok()) return {accepted, {}, {}, {}};
    if (future.wait_for(timeout) != std::future_status::ready)
        return {{media::MediaStatusCode::Timeout, "probe operation timeout"}, {}, {}, {}};
    return future.get();
}

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return predicate();
}

std::size_t directory_entries(const char* path) {
    std::error_code error;
    std::size_t count = 0;
    for (std::filesystem::directory_iterator it(path, error), end;
         !error && it != end; it.increment(error))
        ++count;
    return error ? 0 : count;
}

media::MediaServiceConfig service_config(const Options& options) {
    media::MediaServiceConfig config;
    config.capture.device = options.device;
    config.capture.camera_id = "front";
    config.capture.width = options.width;
    config.capture.height = options.height;
    config.capture.pixel_format = "NV12";
    config.capture.fps = 30;
    config.capture.buffer_count = 4;
    config.capture.poll_timeout_ms = 200;
    config.snapshot_directory = options.output_directory;
    config.recording_directory = options.output_directory;
    config.recorder.fps_numerator = 30;
    config.recorder.fps_denominator = 1;
    config.recorder.bitrate_target = 8'000'000;
    config.recorder.bitrate_min = 7'500'000;
    config.recorder.bitrate_max = 8'500'000;
    config.recorder.gop = 60;
    config.recorder.h264_profile = 100;
    config.recorder.h264_level = 40;
    config.recorder.queue_capacity = 12;
    config.recorder.first_packet_timeout = 5s;
    return config;
}

void print_capture(const media::CaptureStats& stats) {
    const auto elapsed_ns = stats.last_dequeue_steady_ns - stats.first_dequeue_steady_ns;
    const auto elapsed = elapsed_ns > 0
                             ? static_cast<double>(elapsed_ns) / 1'000'000'000.0
                             : 0.0;
    const auto fps = elapsed > 0.0 && stats.frames > 1
                         ? static_cast<double>(stats.frames - 1) / elapsed
                         : 0.0;
    std::cout << "capture_frames=" << stats.frames << '\n'
              << "capture_fps=" << fps << '\n'
              << "stream_epoch=" << stats.stream_epoch << '\n'
              << "sequence_gaps=" << stats.sequence_gap_count << '\n'
              << "poll_timeouts=" << stats.poll_timeouts << '\n'
              << "dqbuf_errors=" << stats.dequeue_errors << '\n'
              << "qbuf_errors=" << stats.queue_errors << '\n';
}

void print_recorder(const media::RecorderStats& stats) {
    const auto elapsed_ns = stats.last_output_steady_ns - stats.first_input_steady_ns;
    const auto elapsed = elapsed_ns > 0
                             ? static_cast<double>(elapsed_ns) / 1'000'000'000.0
                             : 0.0;
    const auto fps = elapsed > 0.0 ? static_cast<double>(stats.encoded_frames) / elapsed : 0.0;
    std::cout << "recording_path=" << stats.output_path << '\n'
              << "input_frames=" << stats.input_frames << '\n'
              << "encoded_frames=" << stats.encoded_frames << '\n'
              << "encoded_fps=" << fps << '\n'
              << "packets=" << stats.packets << '\n'
              << "output_bytes=" << stats.output_bytes << '\n'
              << "queue_peak_depth=" << stats.queue_peak_depth << '\n'
              << "overflow_count=" << stats.overflow_count << '\n'
              << "encoder_errors=" << stats.encoder_errors << '\n'
              << "file_closed=" << (stats.file_closed ? "true" : "false") << '\n';
}

std::map<unsigned, std::uint64_t> scan_annex_b(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("open Annex-B stream for scan");
    constexpr std::size_t scan_limit = 16U * 1024U * 1024U;
    std::vector<std::uint8_t> bytes(scan_limit);
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    std::map<unsigned, std::uint64_t> counts;
    for (std::size_t index = 0; index + 4 < bytes.size(); ++index) {
        std::size_t header = 0;
        if (index + 5 < bytes.size() && bytes[index] == 0 &&
                 bytes[index + 1] == 0 && bytes[index + 2] == 0 &&
                 bytes[index + 3] == 1)
            header = index + 4;
        else if (bytes[index] == 0 && bytes[index + 1] == 0 &&
                 bytes[index + 2] == 1)
            header = index + 3;
        if (header != 0 && header < bytes.size()) {
            ++counts[bytes[header] & 0x1FU];
            index = header;
        }
    }
    std::cout << "nal_scan_bytes=" << bytes.size() << '\n'
              << "nal_sps=" << counts[7] << '\n'
              << "nal_pps=" << counts[8] << '\n'
              << "nal_idr=" << counts[5] << '\n'
              << "nal_slice=" << counts[1] << '\n';
    return counts;
}

std::uint64_t nal_count(const std::map<unsigned, std::uint64_t>& counts,
                        unsigned type) {
    const auto found = counts.find(type);
    return found == counts.end() ? 0U : found->second;
}

int synthetic(const Options& options) {
    std::filesystem::create_directories(options.output_directory);
    const auto path = (std::filesystem::path(options.output_directory) /
                       "synthetic_mpp.h264").string();
    media::CameraFormat format{"front", options.width, options.height, "NV12", 1,
                               options.width, options.width * options.height * 3U / 2U,
                               0, 0, 30, 1, true, "synthetic"};
    media::RecorderConfig config = service_config(options).recorder;
    media::MppH264Recorder recorder;
    const auto start = recorder.start(config, format, path);
    require(start.ok(), "MPP synthetic start: " + start.detail);
    auto frame = std::make_shared<media::CapturedFrame>();
    frame->camera_id = "front";
    frame->width = format.width;
    frame->height = format.height;
    frame->pixel_format = "NV12";
    frame->bytes_per_line = format.bytes_per_line;
    frame->size_image = format.size_image;
    frame->bytes_used = format.size_image;
    frame->stream_epoch = 1;
    frame->payload.assign(format.size_image, 128U);
    std::fill(frame->payload.begin(),
              frame->payload.begin() + static_cast<std::ptrdiff_t>(
                  static_cast<std::size_t>(format.bytes_per_line) * format.height),
              48U);
    for (std::uint64_t index = 0; index < 60; ++index) {
        const auto status = recorder.submit(frame);
        require(status.ok(), "MPP synthetic submit: " + status.detail);
        std::this_thread::sleep_for(10ms);
    }
    require(wait_until([&] { return recorder.stats().encoded_frames >= 60; }, 8s),
            "MPP synthetic encode wait");
    const auto stopped = recorder.stop();
    require(stopped.ok(), "MPP synthetic stop: " + stopped.detail);
    print_recorder(recorder.stats());
    const auto nal = scan_annex_b(path);
    require(nal_count(nal, 7) > 0 && nal_count(nal, 8) > 0 &&
                nal_count(nal, 5) > 0,
            "synthetic NAL validation");
    return 0;
}

std::shared_ptr<media::MediaService> make_service(const Options& options) {
    return std::make_shared<media::MediaService>(
        service_config(options), std::make_unique<media::V4l2MplaneCameraCapture>(),
        std::make_shared<media::PreviewMailbox>(),
        std::make_unique<media::MppH264Recorder>());
}

void verify_recording(const media::MediaOperationResult& stopped) {
    require(stopped.status.ok(), "recording stop: " + stopped.status.detail);
    require(stopped.recorder.file_closed, "recording file not closed");
    require(stopped.recorder.input_frames > 0 &&
                stopped.recorder.encoded_frames == stopped.recorder.input_frames,
            "recording frame accounting");
    require(stopped.recorder.overflow_count == 0 &&
                stopped.recorder.encoder_errors == 0,
            "recording errors");
    require(std::filesystem::is_regular_file(stopped.output_path),
            "recording output missing");
    const auto nal = scan_annex_b(stopped.output_path);
    require(nal_count(nal, 7) > 0 && nal_count(nal, 8) > 0 &&
                nal_count(nal, 5) > 0,
            "real NAL validation");
}

int bounded_record(const Options& options, bool shared) {
    auto service = make_service(options);
    require(service->start().ok(), "media service start");
    if (shared) {
        const auto preview = submit_and_wait(*service, media::MediaOperation::PreviewStart);
        require(preview.status.ok(), "preview start: " + preview.status.detail);
    }
    const auto started = submit_and_wait(*service, media::MediaOperation::RecordingStart);
    require(started.status.ok(), "recording start: " + started.status.detail);
    require(service->recording_active(), "recording not active after first packet");
    require(shared == service->preview_active(), "preview consumer state");
    std::this_thread::sleep_for(std::chrono::seconds(options.seconds));
    const auto stopped = submit_and_wait(*service, media::MediaOperation::RecordingStop, 15s);
    verify_recording(stopped);
    if (shared) {
        require(service->streaming(), "recording stop stopped preview capture");
        const auto preview_stop = submit_and_wait(*service, media::MediaOperation::PreviewStop);
        require(preview_stop.status.ok(), "preview stop: " + preview_stop.status.detail);
    } else {
        require(!service->streaming(), "recording-only retained capture");
        require(!service->preview_active(), "recording-only changed preview state");
    }
    print_capture(service->capture_stats());
    print_recorder(stopped.recorder);
    service->stop();
    require(!service->streaming(), "service capture still active");
    return 0;
}

int restart(const Options& options) {
    const auto cold_fds = directory_entries("/proc/self/fd");
    const auto cold_threads = directory_entries("/proc/self/task");
    std::size_t first_cycle_fds = 0;
    auto service = make_service(options);
    require(service->start().ok(), "media service start");
    for (std::uint32_t cycle = 1; cycle <= options.cycles; ++cycle) {
        const auto started = submit_and_wait(*service, media::MediaOperation::RecordingStart);
        require(started.status.ok(), "cycle start: " + started.status.detail);
        require(wait_until([&] { return service->recorder_stats().encoded_frames >= 10; }, 5s),
                "cycle frame wait");
        const auto stopped = submit_and_wait(*service, media::MediaOperation::RecordingStop, 15s);
        verify_recording(stopped);
        require(!service->streaming(), "cycle retained capture");
        std::cout << "cycle=" << cycle
                  << " epoch=" << service->capture_stats().stream_epoch
                  << " frames=" << stopped.recorder.encoded_frames
                  << " bytes=" << stopped.recorder.output_bytes << '\n';
        if (cycle == 1) first_cycle_fds = directory_entries("/proc/self/fd");
    }
    service->stop();
    const auto final_fds = directory_entries("/proc/self/fd");
    const auto final_threads = directory_entries("/proc/self/task");
    std::cout << "cycles_completed=" << options.cycles << '\n'
              << "cold_fd_count=" << cold_fds << '\n'
              << "first_cycle_fd_count=" << first_cycle_fds << '\n'
              << "final_fd_count=" << final_fds << '\n'
              << "cold_thread_count=" << cold_threads << '\n'
              << "final_thread_count=" << final_threads << '\n';
    require(first_cycle_fds == final_fds,
            "fd count grew after the first MPP initialization");
    require(final_fds <= cold_fds + 1,
            "unexpected process-level MPP fd growth");
    require(cold_threads == final_threads, "thread count changed");
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        std::cout << "mode=" << options.mode << '\n'
                  << "device=" << options.device << '\n'
                  << "seconds=" << options.seconds << '\n'
                  << "cycles=" << options.cycles << '\n';
        if (options.mode == "synthetic") return synthetic(options);
        if (options.mode == "record-only") return bounded_record(options, false);
        if (options.mode == "shared") return bounded_record(options, true);
        return restart(options);
    } catch (const std::exception& error) {
        std::cerr << "media_recording_probe: FAIL: " << error.what() << '\n';
        return 1;
    }
}
