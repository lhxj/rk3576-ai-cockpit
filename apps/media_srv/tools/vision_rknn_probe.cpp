#include "cockpit/infer/rknn_vision_backend.hpp"
#include "cockpit/infer/vision_runtime.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/mpp_h264_encoder.hpp"
#include "cockpit/media/rtsp_server.hpp"
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace infer = cockpit::infer;
namespace media = cockpit::media;

struct Options {
    std::string mode{"vision-only"};
    std::string device;
    std::string model;
    std::string output_directory;
    std::string rtsp_path{"/cam0"};
    std::uint16_t rtsp_port{8554};
    unsigned seconds{10};
    double target_fps{8.0};
};

Options parse(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* name) {
            if (index + 1 >= argc) throw std::runtime_error(std::string("missing ") + name);
            return std::string(argv[++index]);
        };
        if (argument == "--mode") options.mode = value("--mode");
        else if (argument == "--device") options.device = value("--device");
        else if (argument == "--model") options.model = value("--model");
        else if (argument == "--output-dir") options.output_directory = value("--output-dir");
        else if (argument == "--seconds") options.seconds = std::stoul(value("--seconds"));
        else if (argument == "--target-fps") options.target_fps = std::stod(value("--target-fps"));
        else if (argument == "--rtsp-path") options.rtsp_path = value("--rtsp-path");
        else if (argument == "--rtsp-port") {
            const auto port = std::stoul(value("--rtsp-port"));
            if (port > 65535) throw std::runtime_error("invalid --rtsp-port");
            options.rtsp_port = static_cast<std::uint16_t>(port);
        } else throw std::runtime_error("unknown argument: " + argument);
    }
    if (options.device.empty() || options.model.empty() || options.output_directory.empty())
        throw std::runtime_error("--device, --model and --output-dir are required");
    if (options.mode != "vision-only" && options.mode != "preview-vision" &&
        options.mode != "shared")
        throw std::runtime_error("--mode vision-only|preview-vision|shared");
    if (options.seconds == 0 || options.target_fps <= 0.0)
        throw std::runtime_error("duration and target FPS must be positive");
    return options;
}

void require(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error(detail);
}

media::MediaOperationResult submit(media::MediaService& service,
                                   media::MediaOperation operation) {
    auto promise = std::make_shared<std::promise<media::MediaOperationResult>>();
    auto future = promise->get_future();
    const auto accepted = service.submit(operation, [promise](auto result) {
        try { promise->set_value(std::move(result)); } catch (...) {}
    });
    if (!accepted.ok()) return {accepted};
    if (future.wait_for(15s) != std::future_status::ready)
        return {{media::MediaStatusCode::Timeout, "operation timeout"}};
    return future.get();
}

void require_operation(media::MediaService& service, media::MediaOperation operation,
                       const char* name) {
    const auto result = submit(service, operation);
    std::cout << "operation=" << name << " status="
              << static_cast<unsigned>(result.status.code)
              << " detail=" << result.status.detail << '\n';
    require(result.status.ok(), std::string(name) + ": " + result.status.detail);
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse(argc, argv);
        std::filesystem::create_directories(options.output_directory);
        infer::SerialInferenceScheduler scheduler;
        infer::RknnVisionBackend backend(scheduler,
            {options.model, "MobileNetV1 RK3576", 5});
        infer::VisionRuntime runtime(backend, {options.target_fps, 2, 114});

        media::MediaServiceConfig config;
        config.capture.device = options.device;
        config.capture.camera_id = "front";
        config.capture.width = 1632;
        config.capture.height = 1224;
        config.capture.pixel_format = "NV12";
        config.capture.fps = 30;
        config.capture.buffer_count = 4;
        config.capture.poll_timeout_ms = 200;
        config.snapshot_directory = options.output_directory;
        config.recording_directory = options.output_directory;
        config.encoder.first_packet_timeout = 5s;
        config.rtsp.port = options.rtsp_port;
        config.rtsp.path = options.rtsp_path;
        media::MediaService service(
            config, std::make_unique<media::V4l2MplaneCameraCapture>(),
            std::make_shared<media::PreviewMailbox>(),
            std::make_unique<media::MppH264Encoder>(),
            std::make_unique<media::FileRecordingSink>(),
            std::make_unique<media::RtspServer>());

        service.set_vision_frame_callback(
            [&runtime](std::shared_ptr<const media::CapturedFrame> captured) {
                if (!captured) return;
                infer::VisionFrame input;
                input.camera_id = captured->camera_id;
                input.width = captured->width;
                input.height = captured->height;
                input.pixel_format = captured->pixel_format;
                input.bytes_per_line = captured->bytes_per_line;
                input.bytes_used = captured->bytes_used;
                input.sequence = captured->sequence;
                input.stream_epoch = captured->stream_epoch;
                input.capture_timestamp_ns = captured->capture_timestamp_ns;
                input.dequeue_steady_timestamp_ns = captured->dequeue_steady_timestamp_ns;
                const auto* payload = &captured->payload;
                input.payload = std::shared_ptr<const std::vector<std::uint8_t>>(
                    std::move(captured), payload);
                (void)runtime.submit(std::move(input));
            });
        require(service.start().ok(), "media service start");
        require(runtime.start().ok(), "vision runtime/model start");
        const auto loaded = runtime.snapshot();
        std::cout << "model=" << loaded.backend.model_name
                  << " runtime=" << loaded.backend.runtime_version
                  << " driver=" << loaded.backend.driver_version
                  << " input=" << loaded.backend.input_width << 'x'
                  << loaded.backend.input_height << 'x' << loaded.backend.input_channels
                  << " output=" << loaded.backend.output_shape
                  << " model_load_ms=" << loaded.backend.model_load_ms << '\n';

        require_operation(service, media::MediaOperation::VisionStart, "vision-start");
        const bool preview = options.mode != "vision-only";
        const bool shared = options.mode == "shared";
        if (preview) require_operation(service, media::MediaOperation::PreviewStart,
                                       "preview-start");
        if (shared) {
            require_operation(service, media::MediaOperation::RecordingStart,
                              "recording-start");
            require_operation(service, media::MediaOperation::RtspStart, "rtsp-start");
            std::cout << "rtsp_url=rtsp://<wlan0-ip>:"
                      << service.rtsp_stats().listen_port << options.rtsp_path << '\n';
        }

        std::uint64_t preview_frames = 0;
        std::uint64_t last_delivery = 0;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(options.seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            if (preview) {
                media::PreviewDelivery delivery;
                if (service.preview_mailbox()->wait_next(last_delivery, 100ms, delivery)) {
                    last_delivery = delivery.delivery_id;
                    ++preview_frames;
                }
            } else {
                std::this_thread::sleep_for(100ms);
            }
        }

        if (shared) {
            require_operation(service, media::MediaOperation::RtspStop, "rtsp-stop");
            require_operation(service, media::MediaOperation::RecordingStop,
                              "recording-stop");
        }
        if (preview) require_operation(service, media::MediaOperation::PreviewStop,
                                       "preview-stop");
        require_operation(service, media::MediaOperation::VisionStop, "vision-stop");
        require(runtime.stop().ok(), "vision runtime stop");
        service.set_vision_frame_callback({});
        service.stop();

        const auto vision = runtime.snapshot();
        const auto capture = service.capture_stats();
        const auto encoder = service.encoder_stats();
        const auto recorder = service.recorder_stats();
        const auto rtsp = service.rtsp_stats();
        std::cout << "vision_input_frames=" << vision.metrics.vision_input_frames << '\n'
                  << "vision_inferred_frames=" << vision.metrics.vision_inferred_frames << '\n'
                  << "vision_drop_count=" << vision.metrics.vision_drop_count << '\n'
                  << "vision_sampling_drop_count=" << vision.metrics.sampling_drop_count << '\n'
                  << "vision_queue_drop_count=" << vision.metrics.queue_drop_count << '\n'
                  << "vision_stale_epoch_drop_count=" << vision.metrics.stale_epoch_drop_count << '\n'
                  << "vision_queue_peak=" << vision.metrics.queue_peak << '\n'
                  << "vision_fps=" << vision.metrics.vision_fps << '\n'
                  << "preprocess_ms=" << vision.metrics.preprocess_ms_average << '\n'
                  << "inference_ms=" << vision.metrics.inference_ms_average << '\n'
                  << "postprocess_ms=" << vision.metrics.postprocess_ms_average << '\n'
                  << "end_to_end_ms=" << vision.metrics.end_to_end_ms_average << '\n'
                  << "capture_frames=" << capture.frames << '\n'
                  << "capture_stream_epoch=" << capture.stream_epoch << '\n'
                  << "sequence_gaps=" << capture.sequence_gap_count << '\n'
                  << "dqbuf_errors=" << capture.dequeue_errors << '\n'
                  << "qbuf_errors=" << capture.queue_errors << '\n'
                  << "preview_frames=" << preview_frames << '\n'
                  << "encoded_frames=" << encoder.encoded_frames << '\n'
                  << "encoder_instances=" << encoder.start_count << '\n'
                  << "recording_overflow=" << recorder.overflow_count << '\n'
                  << "recording_path=" << recorder.output_path << '\n'
                  << "rtp_packet_count=" << rtsp.rtp_packet_count << '\n'
                  << "rtp_drop_count=" << rtsp.rtp_drop_count << '\n';
        require(vision.metrics.vision_inferred_frames > 0, "no vision result");
        require(vision.metrics.queue_peak <= 2, "vision queue exceeded capacity");
        require(capture.stream_epoch == 1, "CAM0 was restarted during the scenario");
        if (shared) {
            require(encoder.start_count == 1, "multiple MPP encoders");
            require(recorder.overflow_count == 0, "recording queue overflow");
        }
        std::cout << "VISION_RKNN_CAM0_PROBE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "vision_rknn_probe: FAIL: " << error.what() << '\n';
        return 1;
    }
}
