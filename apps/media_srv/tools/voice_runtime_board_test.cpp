#include "cockpit/audio/alsa_capture.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/mpp_h264_encoder.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/media/v4l2_mplane_camera_capture.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"
#include "cockpit/vehicle/voice_command_sink.hpp"
#include "cockpit/voice/sherpa_asr.hpp"
#include "cockpit/voice/sherpa_vad.hpp"
#include "cockpit/voice/voice_runtime.hpp"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace {
using namespace cockpit;

struct Options {
    std::string camera_device;
    std::string audio_device{"hw:0,0"};
    std::string model_config;
    std::string model_dir;
    std::string vad_model;
    std::string recording_directory;
    int listen_seconds{5};
    int record_seconds{3};
};

int positive(const std::string& value, const char* name, int high) {
    std::size_t used = 0;
    const int parsed = std::stoi(value, &used);
    if (used != value.size() || parsed < 1 || parsed > high)
        throw std::runtime_error(std::string("invalid ") + name);
    return parsed;
}

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) throw std::runtime_error("missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--camera-device") options.camera_device = value;
        else if (key == "--audio-device") options.audio_device = value;
        else if (key == "--model-config") options.model_config = value;
        else if (key == "--model-dir") options.model_dir = value;
        else if (key == "--vad-model") options.vad_model = value;
        else if (key == "--recording-dir") options.recording_directory = value;
        else if (key == "--listen-seconds")
            options.listen_seconds = positive(value, "listen seconds", 60);
        else if (key == "--record-seconds")
            options.record_seconds = positive(value, "record seconds", 30);
        else throw std::runtime_error("unknown option " + key);
    }
    if (options.camera_device.empty() || options.model_config.empty() ||
        options.model_dir.empty() || options.vad_model.empty() ||
        options.recording_directory.empty())
        throw std::runtime_error("camera/model/VAD/recording options are required");
    return options;
}

void require(bool value, const std::string& detail) {
    if (!value) throw std::runtime_error(detail);
}

std::size_t thread_count() {
    std::error_code error;
    std::size_t count = 0;
    for (std::filesystem::directory_iterator it("/proc/self/task", error), end;
         !error && it != end; it.increment(error)) ++count;
    return error ? 0 : count;
}

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

class Reports {
public:
    void add(const voice::IntentDispatchReport& report) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(report);
        ready_.notify_all();
    }
    bool wait(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, 10s, [&] { return values_.size() >= count; });
    }
private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<voice::IntentDispatchReport> values_;
};

media::MediaServiceConfig media_config(const Options& options) {
    media::MediaServiceConfig config;
    config.capture.device = options.camera_device;
    config.capture.camera_id = "front";
    config.capture.width = 1632;
    config.capture.height = 1224;
    config.capture.pixel_format = "NV12";
    config.capture.fps = 30;
    config.capture.buffer_count = 4;
    config.capture.poll_timeout_ms = 200;
    config.recording_directory = options.recording_directory;
    config.snapshot_directory = options.recording_directory;
    config.recording_packet_queue_capacity = 64;
    return config;
}

void reopen_camera(const Options& options) {
    media::V4l2MplaneCameraCapture capture;
    auto status = capture.open_device(options.camera_device);
    require(status.ok(), "CAMERA_REOPEN open: " + status.detail);
    status = capture.configure(media_config(options).capture);
    require(status.ok(), "CAMERA_REOPEN configure: " + status.detail);
    status = capture.start([](media::CapturedFrame) {});
    require(status.ok(), "CAMERA_REOPEN start: " + status.detail);
    require(wait_until([&] { return capture.stats().frames >= 3; }, 3s),
            "CAMERA_REOPEN no frames");
    require(capture.stop().ok(), "CAMERA_REOPEN stop");
    capture.close_device();
    std::cout << "CAMERA_REOPEN=PASS\n";
}

void reopen_audio(const Options& options) {
    audio::AlsaAudioCapture capture(options.audio_device);
    const auto status = capture.start({});
    require(status.ok(), "AUDIO_REOPEN start: " + status.detail);
    const auto sample = capture.read(500ms);
    require(sample.status.ok() || sample.status.code == protocol::StatusCode::TIMEOUT,
            "AUDIO_REOPEN read: " + sample.status.detail);
    capture.stop();
    std::cout << "AUDIO_REOPEN=PASS\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse(argc, argv);
        const auto initial_threads = thread_count();

        voice::SherpaModelFiles files;
        auto status = voice::read_sherpa_model_manifest(
            options.model_config, options.model_dir, files);
        require(status.ok(), "model manifest: " + status.detail);
        voice::SherpaAsrBackend asr(voice::make_sherpa_c_api_engine());
        status = asr.load(files);
        require(status.ok(), "ASR model load: " + status.detail);

        auto service = std::make_shared<media::MediaService>(
            media_config(options), std::make_unique<media::V4l2MplaneCameraCapture>(),
            std::make_shared<media::PreviewMailbox>(),
            std::make_unique<media::MppH264Encoder>(),
            std::make_unique<media::FileRecordingSink>());
        auto media_adapter = std::make_shared<media::RealMediaServiceAdapter>(service);
        auto voice_adapter = std::make_shared<vehicle::MockVoiceAdapter>();
        auto rtos_adapter = std::make_shared<vehicle::MockRtosAdapter>();
        auto system_adapter = std::make_shared<vehicle::MockSystemAdapter>();
        auto registry = std::make_shared<vehicle::ServiceRegistry>();
        auto clock = std::make_shared<vehicle::SystemClock>();
        constexpr protocol::BootEpoch epoch = 20261002;
        vehicle::VehicleCore core({epoch, 16, 16, 128, 2ms},
            {media_adapter, voice_adapter, rtos_adapter, system_adapter}, registry, clock);
        vehicle::InProcessVehicleCoreClient client(core);
        voice::VoiceSessionController controller(epoch);
        voice::DeterministicIntentRouter router;
        vehicle::VehicleCommandSinkAdapter sink(client, clock, epoch);
        audio::AlsaAudioCapture capture(options.audio_device);
        voice::SherpaVadBackend vad;
        voice::VoiceRuntimeConfig runtime_config;
        runtime_config.vad.model_path = options.vad_model;
        Reports reports;
        voice::VoiceRuntime runtime(capture, vad, asr, controller, router, sink,
            [clock] { return clock->now_ms(); }, runtime_config,
            [&](const voice::IntentDispatchReport& report) { reports.add(report); },
            [&] { return capture.metrics().xrun_count; });

        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::OFFLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        media_adapter->set_runtime_state_callback(
            [&](vehicle::CommandType type, vehicle::AdapterResult result) {
                (void)core.report_runtime_result(type, std::move(result));
            });

        require(service->start().ok(), "MediaService start");
        require(core.start().ok(), "VehicleCore start");
        require(runtime.start().ok(), "VoiceRuntime start");
        const auto negotiated = capture.actual_format();
        require(negotiated.has_value(), "ALSA negotiated format absent");
        std::cout << "VOICE_RUNTIME state=" << voice::voice_runtime_state_name(runtime.state())
                  << " live_seconds=" << options.listen_seconds
                  << " asr_loaded=" << (asr.loaded() ? 1 : 0)
                  << " vad_load_count=" << vad.load_count()
                  << " sample_rate=" << negotiated->sample_rate
                  << " channels=" << negotiated->channels
                  << " period_frames=" << negotiated->frames_per_buffer << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(options.listen_seconds));

        std::size_t report_count = 0;
        const auto invoke = [&](const std::string& text) {
            require(runtime.inject_final_for_test(text).ok(), "synthetic FINAL " + text);
            require(reports.wait(++report_count), "intent report " + text);
            const auto submission = sink.last_submission();
            require(submission && submission->accepted() && submission->ack_emitted,
                    "ACK " + text);
            require(submission->result.wait_for(10s) == std::future_status::ready,
                    "RESULT timeout " + text);
            const auto result = submission->result.get();
            require(result.status.ok(), "RESULT failure " + text + ": " + result.status.detail);
            std::cout << "SYNTHETIC_FINAL text=" << text
                      << " ack=" << submission->ack.lifecycle_sequence
                      << " result=" << result.lifecycle_sequence << '\n';
        };

        invoke("打开摄像头");
        require(wait_until([&] { return service->capture_stats().frames >= 3; }, 5s),
                "real CAM0 produced no frames");
        const auto preview_frames = service->capture_stats().frames;
        invoke("关闭摄像头");
        require(!service->streaming(), "CAM0 still streaming after close");

        invoke("开始录像");
        std::this_thread::sleep_for(std::chrono::seconds(options.record_seconds));
        invoke("停止录像");
        const auto recorder = service->recorder_stats();
        require(recorder.file_closed && recorder.packets > 0 && recorder.output_bytes > 0,
                "recording file not finalized");
        std::cout << "REAL_MEDIA preview_frames=" << preview_frames
                  << " recording_packets=" << recorder.packets
                  << " recording_bytes=" << recorder.output_bytes
                  << " recording_file=" << recorder.output_path << '\n';

        require(runtime.stop().ok(), "VoiceRuntime stop");
        const auto metrics = runtime.metrics();
        const auto audio_metrics = capture.metrics();
        core.stop();
        service->stop();
        asr.unload();
        const auto final_threads = thread_count();
        require(final_threads <= initial_threads, "residual application threads");
        std::cout << "VOICE_RUNTIME_METRICS utterances=" << metrics.utterance_count
                  << " finals=" << metrics.final_count
                  << " matches=" << metrics.intent_match_count
                  << " no_match=" << metrics.no_match_count
                  << " rejected=" << metrics.rejected_count
                  << " duplicates=" << metrics.duplicate_final_count
                  << " dispatch_queue_peak=" << metrics.dispatch_queue_peak
                  << " audio_queue_peak=" << metrics.audio_queue_peak
                  << " captured_frames=" << audio_metrics.captured_frames
                  << " xrun_count=" << metrics.xrun_count
                  << " audio_overflow_count=" << metrics.audio_overflow_count
                  << " threads_before=" << initial_threads
                  << " threads_after=" << final_threads << '\n';
        reopen_camera(options);
        reopen_audio(options);
        std::cout << "VOICE_RUNTIME_ORCHESTRATION_BOARD_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "voice_runtime_board_test: FAIL: " << error.what() << '\n';
        return 1;
    }
}
