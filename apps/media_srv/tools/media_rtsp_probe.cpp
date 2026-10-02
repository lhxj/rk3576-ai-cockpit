#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/mpp_h264_encoder.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/media/rtsp_server.hpp"
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace media = cockpit::media;
namespace vehicle = cockpit::vehicle;
namespace protocol = cockpit::protocol;

struct Options {
    std::string mode{"rtsp-only"};
    std::string device;
    std::string output_directory;
    std::string path{"/cam0"};
    std::uint16_t port{8554};
    unsigned seconds{10};
    unsigned cycles{20};
};

Options parse(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing ") + name);
            return argv[++i];
        };
        if (arg == "--mode") options.mode = value("--mode");
        else if (arg == "--device") options.device = value("--device");
        else if (arg == "--output-dir") options.output_directory = value("--output-dir");
        else if (arg == "--port") {
            const auto port = std::stoul(value("--port"));
            if (port > 65535U) throw std::runtime_error("--port must be 0..65535");
            options.port = static_cast<std::uint16_t>(port);
        }
        else if (arg == "--path") options.path = value("--path");
        else if (arg == "--seconds") options.seconds = static_cast<unsigned>(std::stoul(value("--seconds")));
        else if (arg == "--cycles") options.cycles = static_cast<unsigned>(std::stoul(value("--cycles")));
        else throw std::runtime_error("unknown argument: " + arg);
    }
    if (options.device.empty() || options.output_directory.empty())
        throw std::runtime_error("--device and --output-dir are required");
    if (options.path.empty() || options.path.front() != '/')
        throw std::runtime_error("--path must start with /");
    if (options.mode != "rtsp-only" && options.mode != "preview-rtsp" &&
        options.mode != "shared" && options.mode != "restart")
        throw std::runtime_error("--mode rtsp-only|preview-rtsp|shared|restart");
    return options;
}

void require(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error(detail);
}

vehicle::VehicleCommand command(vehicle::VehicleCore& core, vehicle::IClock& clock,
                                protocol::RequestId id, vehicle::CommandType type) {
    vehicle::VehicleCommand value;
    value.request_id = id;
    value.boot_epoch = core.boot_epoch();
    value.deadline_ms = clock.now_ms() + 10000;
    value.source = vehicle::CommandSource::TEST;
    value.command_type = type;
    return value;
}

vehicle::CommandResult execute(vehicle::InProcessVehicleCoreClient& client,
                               vehicle::VehicleCore& core, vehicle::IClock& clock,
                               protocol::RequestId id, vehicle::CommandType type) {
    auto submission = client.send_command(command(core, clock, id, type));
    require(submission.accepted(), "command rejected");
    std::cout << "command=" << static_cast<unsigned>(type)
              << " ack=" << submission.ack.lifecycle_sequence << '\n';
    require(submission.result.wait_for(15s) == std::future_status::ready,
            "command RESULT timeout");
    auto result = submission.result.get();
    std::cout << "command=" << static_cast<unsigned>(type)
              << " result=" << result.lifecycle_sequence
              << " status=" << static_cast<unsigned>(result.status.code)
              << " detail=" << result.status.detail << '\n';
    require(result.status.ok(), "command failed: " + result.status.detail);
    return result;
}

void print_stats(const media::MediaService& service, std::uint64_t preview_frames) {
    const auto capture = service.capture_stats();
    const auto encoder = service.encoder_stats();
    const auto rtsp = service.rtsp_stats();
    const auto recorder = service.recorder_stats();
    const auto capture_elapsed = capture.last_dequeue_steady_ns - capture.first_dequeue_steady_ns;
    const auto encoded_elapsed = encoder.last_output_steady_ns - encoder.first_input_steady_ns;
    const double capture_fps = capture_elapsed > 0 && capture.frames > 1
        ? static_cast<double>(capture.frames - 1) * 1e9 / static_cast<double>(capture_elapsed) : 0.0;
    const double encoded_fps = encoded_elapsed > 0
        ? static_cast<double>(encoder.encoded_frames) * 1e9 / static_cast<double>(encoded_elapsed) : 0.0;
    std::cout << "capture_frames=" << capture.frames << '\n'
              << "capture_fps=" << capture_fps << '\n'
              << "preview_frames=" << preview_frames << '\n'
              << "encoded_frames=" << encoder.encoded_frames << '\n'
              << "encoded_fps=" << encoded_fps << '\n'
              << "encoder_instances=" << encoder.start_count << '\n'
              << "encoder_queue_peak=" << encoder.queue_peak_depth << '\n'
              << "encoder_overflow=" << encoder.overflow_count << '\n'
              << "encoder_errors=" << encoder.encoder_errors << '\n'
              << "rtp_packet_count=" << rtsp.rtp_packet_count << '\n'
              << "rtp_drop_count=" << rtsp.rtp_drop_count << '\n'
              << "rtsp_queue_peak=" << rtsp.queue_peak_depth << '\n'
              << "client_connect_count=" << rtsp.client_connect_count << '\n'
              << "client_reconnect_count=" << rtsp.client_reconnect_count << '\n'
              << "client_join_to_first_idr_ms=" << rtsp.client_join_to_first_idr_ms << '\n'
              << "recording_queue_peak=" << recorder.queue_peak_depth << '\n'
              << "recording_overflow=" << recorder.overflow_count << '\n'
              << "recording_path=" << recorder.output_path << '\n'
              << "sequence_gaps=" << capture.sequence_gap_count << '\n'
              << "poll_timeouts=" << capture.poll_timeouts << '\n'
              << "dqbuf_errors=" << capture.dequeue_errors << '\n'
              << "qbuf_errors=" << capture.queue_errors << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse(argc, argv);
        std::filesystem::create_directories(options.output_directory);
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
        config.rtsp.port = options.port;
        config.rtsp.path = options.path;
        auto service = std::make_shared<media::MediaService>(
            config, std::make_unique<media::V4l2MplaneCameraCapture>(),
            std::make_shared<media::PreviewMailbox>(),
            std::make_unique<media::MppH264Encoder>(),
            std::make_unique<media::FileRecordingSink>(),
            std::make_unique<media::RtspServer>());
        auto adapter = std::make_shared<media::RealMediaServiceAdapter>(service);
        auto voice = std::make_shared<vehicle::MockVoiceAdapter>();
        auto rtos = std::make_shared<vehicle::MockRtosAdapter>();
        auto system = std::make_shared<vehicle::MockSystemAdapter>();
        auto registry = std::make_shared<vehicle::ServiceRegistry>();
        auto clock = std::make_shared<vehicle::SystemClock>();
        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        vehicle::VehicleCore core({20261002, 32, 32, 128, 2ms},
            {adapter, voice, rtos, system}, registry, clock);
        adapter->set_runtime_state_callback([&core](vehicle::CommandType type,
                                                    vehicle::AdapterResult result) {
            (void)core.report_runtime_result(type, std::move(result));
        });
        vehicle::InProcessVehicleCoreClient client(core);
        require(service->start().ok(), "media service start");
        require(core.start().ok(), "vehicle core start");
        protocol::RequestId request = 1;
        std::uint64_t preview_frames = 0;
        std::atomic_bool stop_preview{false};
        std::thread preview_consumer;
        const bool preview = options.mode == "preview-rtsp" || options.mode == "shared";
        if (preview) {
            execute(client, core, *clock, request++, vehicle::CommandType::CAMERA_PREVIEW_START);
            const auto mailbox = service->preview_mailbox();
            preview_consumer = std::thread([&] {
                std::uint64_t last = 0;
                while (!stop_preview.load()) {
                    media::PreviewDelivery delivery;
                    if (mailbox->wait_next(last, 100ms, delivery)) {
                        last = delivery.delivery_id;
                        ++preview_frames;
                    }
                }
            });
        }
        const bool recording = options.mode == "shared";
        if (recording)
            execute(client, core, *clock, request++, vehicle::CommandType::RECORDING_START);
        if (options.mode == "restart") {
            for (unsigned cycle = 1; cycle <= options.cycles; ++cycle) {
                execute(client, core, *clock, request++, vehicle::CommandType::RTSP_START);
                execute(client, core, *clock, request++, vehicle::CommandType::RTSP_STOP);
                std::cout << "cycle=" << cycle << '\n';
            }
        } else {
            execute(client, core, *clock, request++, vehicle::CommandType::RTSP_START);
            const auto actual_port = service->rtsp_stats().listen_port;
            std::cout << "rtsp_url=rtsp://<wlan0-ip>:" << actual_port << options.path << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(options.seconds));
            execute(client, core, *clock, request++, vehicle::CommandType::RTSP_STOP);
        }
        if (recording)
            execute(client, core, *clock, request++, vehicle::CommandType::RECORDING_STOP);
        if (preview)
            execute(client, core, *clock, request++, vehicle::CommandType::CAMERA_PREVIEW_STOP);
        stop_preview.store(true);
        if (preview_consumer.joinable()) preview_consumer.join();
        print_stats(*service, preview_frames);
        core.stop();
        service->stop();
        require(!service->streaming(), "camera still streaming");
        std::cout << "MEDIA_CAM0_RTSP_PROBE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "media_rtsp_probe: FAIL: " << error.what() << '\n';
        return 1;
    }
}
