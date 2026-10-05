// SPDX-License-Identifier: MIT
// Test-only orchestration of unchanged Application components. No synthetic FINAL.
#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/coexistence_consumer_gate.h"
#include "cockpit/audio/alsa_capture.hpp"
#include "cockpit/infer/rknn_vision_backend.hpp"
#include "cockpit/vehicle/voice_command_sink.hpp"
#include "cockpit/voice/sherpa_asr.hpp"
#include "cockpit/voice/sherpa_vad.hpp"
#include "cockpit/voice/voice_runtime.hpp"

// Parse the existing ASR class's private emit() before Qt's emit keyword macro.
#include "cockpit_ui/main_window.h"
#include "cockpit_ui/vision_ui_backend.h"

#include <QApplication>
#include <QMetaObject>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
using namespace cockpit;

struct Options {
    std::string camera, vision_model, asr_config, asr_dir, vad_model, output_dir;
    std::string audio{"hw:CARD=rockchipes8388,DEV=0"};
    unsigned seconds{300};
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Options parse(int argc, char** argv) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string key = argv[index];
        require(index + 1 < argc, "missing option value: " + key);
        const std::string value = argv[++index];
        if (key == "--camera-device") result.camera = value;
        else if (key == "--vision-model") result.vision_model = value;
        else if (key == "--asr-config") result.asr_config = value;
        else if (key == "--asr-dir") result.asr_dir = value;
        else if (key == "--vad-model") result.vad_model = value;
        else if (key == "--output-dir") result.output_dir = value;
        else if (key == "--audio-device") result.audio = value;
        else if (key == "--seconds") {
            std::size_t used = 0;
            result.seconds = static_cast<unsigned>(std::stoul(value, &used));
            require(used == value.size(), "invalid seconds");
        } else throw std::runtime_error("unknown option: " + key);
    }
    require(!result.camera.empty() && !result.vision_model.empty() &&
            !result.asr_config.empty() && !result.asr_dir.empty() &&
            !result.vad_model.empty() && !result.output_dir.empty(), "all asset/device paths required");
    require(result.seconds >= 300 && result.seconds <= 360, "coexistence must last 300..360 seconds");
    return result;
}

std::map<std::string,std::string> health() {
    std::ifstream input("/sys/module/rk3576_amp_health_test/parameters/health_status");
    require(input.good(), "real RPMsg health_status missing");
    std::string line;
    std::getline(input,line);
    require(!line.empty() && line.size() < 2048, "invalid health status length");
    std::istringstream fields(line);
    std::map<std::string,std::string> values;
    std::string field;
    while (fields >> field) {
        const auto split = field.find('=');
        require(split != std::string::npos, "invalid health field");
        require(values.emplace(field.substr(0,split),field.substr(split+1)).second,
                "duplicate health field");
    }
    return values;
}

std::uint64_t number(const std::map<std::string,std::string>& values, const char* key) {
    const auto& text = values.at(key);
    std::size_t used = 0;
    const auto result = std::stoull(text,&used);
    require(used == text.size() && text[0] != '-', "invalid numeric health field");
    return result;
}

class HealthGate {
public:
    void check(bool require_progress) {
        const auto values = health();
        require(number(values,"bound") == 1 && number(values,"hello_ack") == 1,
                "RPMsg not bound/handshaken");
        require(values.at("phase") == "READY" || values.at("phase") == "WAIT_PONG",
                "RPMsg unhealthy/expired phase " + values.at("phase"));
        require(number(values,"timeout") == 0 && number(values,"error") == 0,
                "RPMsg timeout/error");
        const auto ping = number(values,"ping");
        const auto pong = number(values,"pong");
        require(pong > 0 && ping >= pong && ping-pong <= 1, "RPMsg in-flight/counter violation");
        require(number(values,"last_pong_age_ms") < 5000 &&
                number(values,"elapsed_ms") < 840000, "RPMsg stale/expired test window");
        require(!require_progress || pong > last_pong_, "RPMsg real PONG stopped advancing");
        last_pong_ = pong;
        std::cout << "RPMSG_HEALTH ping=" << ping << " pong=" << pong
                  << " timeout=0 error=0 rtt_ms=" << number(values,"rtt_ms")
                  << " age_ms=" << number(values,"last_pong_age_ms") << '\n';
    }
private:
    std::uint64_t last_pong_{0};
};

template<class Predicate>
void wait_until(Predicate predicate, std::atomic<bool>& cancelled,
                std::chrono::milliseconds timeout, const char* label) {
    const auto limit = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < limit) {
        require(!cancelled.load(), "UI closed during test");
        if (predicate()) return;
        std::this_thread::sleep_for(50ms);
    }
    require(predicate(), std::string("timeout: ") + label);
}

void media_operation(media::MediaService& service, media::MediaOperation operation) {
    auto completion = std::make_shared<std::promise<media::MediaOperationResult>>();
    auto future = completion->get_future();
    require(service.submit(operation,[completion](auto result) {
        try { completion->set_value(std::move(result)); } catch (...) {}
    }).ok(), "media operation rejected");
    require(future.wait_for(10s) == std::future_status::ready, "media RESULT timeout");
    const auto result = future.get();
    require(result.status.ok(), "media RESULT failed: " + result.status.detail);
}

void command(ui::CoreIntegrationRuntime& runtime, vehicle::CommandType type,
             protocol::RequestId& next, const std::shared_ptr<vehicle::SystemClock>& clock) {
    vehicle::VehicleCommand request;
    request.request_id = next++;
    request.boot_epoch = runtime.client().boot_epoch();
    request.deadline_ms = clock->now_ms() + 15000;
    request.source = vehicle::CommandSource::TEST;
    request.command_type = type;
    const auto submitted = runtime.client().send_command(request);
    require(submitted.accepted() && submitted.ack_emitted,"VehicleCore ACK rejected");
    require(submitted.result.wait_for(15s) == std::future_status::ready,"VehicleCore RESULT timeout");
    const auto result = submitted.result.get();
    require(result.status.ok() && !result.simulated,"VehicleCore real RESULT failed: " + result.status.detail);
    require(result.lifecycle_sequence > submitted.ack.lifecycle_sequence,"ACK is not RESULT");
    std::cout << "CORE_COMMAND type=" << static_cast<unsigned>(type)
              << " ack=" << submitted.ack.lifecycle_sequence
              << " result=" << result.lifecycle_sequence << " status=SUCCESS source=RUNTIME\n";
}

void sensor_metrics(ui::CoreIntegrationRuntime& runtime) {
 const auto s=runtime.core().get_snapshot().sensor_state;
 static std::uint64_t previous_seq=0;require(s.sample_seq>previous_seq,"sensor sample stopped progressing");previous_seq=s.sample_seq;
 std::cout<<"SENSOR_COEX_METRICS session="<<s.session_id<<" epoch="<<s.remote_epoch<<" subscription="<<s.subscription_id<<" seq="<<s.sample_seq<<" pubseq="<<s.publish_seq<<" age_ms="<<s.age_ms<<" sample_errors="<<s.sample_errors<<" overwrites="<<s.latest_overwrites<<" send_failures="<<s.send_failures<<" gaps="<<s.sequence_gaps<<" duplicate="<<s.duplicate<<" protocol_errors="<<s.protocol_errors<<" linux_overwrites="<<s.linux_sample_overwrites<<" linux_control_drops="<<s.linux_control_drops<<" linux_malformed="<<s.linux_malformed<<" linux_send_failures="<<s.linux_send_failures<<" control_drops="<<s.control_drops<<" lease_active="<<s.subscription_active<<"\n";
 require(s.source==vehicle::StateSource::RUNTIME && s.data==vehicle::SensorDataCondition::VALID && s.subscription_active && s.rtos_online && s.rpmsg_online && s.mpu_available,"sensor stream offline/stale");
 require(!s.sample_errors && !s.protocol_errors && !s.send_failures && !s.linux_malformed && !s.linux_send_failures,"sensor errors during coexistence");
}

void metrics(media::MediaService& service, infer::VisionRuntime& vision,
             voice::VoiceRuntime& voice, audio::AlsaAudioCapture& audio) {
    const auto capture = service.capture_stats();
    const auto recording = service.recorder_stats();
    const auto encoder = service.encoder_stats();
    const auto rtsp = service.rtsp_stats();
    const auto image = vision.snapshot();
    const auto speech = voice.metrics();
    const auto sound = audio.metrics();
    const std::array<std::uint64_t,5> progress{capture.frames,encoder.encoded_frames,recording.packets,image.metrics.vision_inferred_frames,sound.captured_frames};
    static std::array<std::uint64_t,5> previous{};static bool established=false;
    if(established)for(std::size_t i=0;i<progress.size();i++)require(progress[i]>previous[i],"media/vision/audio counter stopped during full load");
    previous=progress;established=true;
    const auto elapsed = capture.last_dequeue_steady_ns - capture.first_dequeue_steady_ns;
    const auto fps = elapsed > 0 && capture.frames > 1
        ? (capture.frames - 1) * 1e9 / static_cast<double>(elapsed) : 0.0;
    std::cout << "APPLICATION_METRICS capture_frames=" << capture.frames << " capture_fps=" << fps
              << " sequence_gap=" << capture.sequence_gap_count
              << " dqbuf_error=" << capture.dequeue_errors << " qbuf_error=" << capture.queue_errors
              << " record_overflow=" << recording.overflow_count
              << " record_packets=" << recording.packets << " record_bytes=" << recording.output_bytes
              << " encoder_instances=" << encoder.start_count << " encoded_frames=" << encoder.encoded_frames
              << " encoder_error=" << encoder.encoder_errors << " encoder_overflow=" << encoder.overflow_count
              << " rtp_packet=" << rtsp.rtp_packet_count << " rtp_drop=" << rtsp.rtp_drop_count
              << " vision_fps=" << image.metrics.vision_fps
              << " vision_frames=" << image.metrics.vision_inferred_frames
              << " vision_drop=" << image.metrics.vision_drop_count
              << " vision_queue_drop=" << image.metrics.queue_drop_count
              << " vision_queue_peak=" << image.metrics.queue_peak
              << " audio_frames=" << sound.captured_frames << " audio_xrun=" << speech.xrun_count
              << " audio_overflow=" << speech.audio_overflow_count
              << " audio_queue_peak=" << speech.audio_queue_peak << '\n';
    require(capture.dequeue_errors == 0 && capture.queue_errors == 0, "V4L2 errors");
    require(recording.overflow_count == 0 && recording.last_error.empty(), "Recording overflow/error");
    require(encoder.encoder_errors == 0 && encoder.overflow_count == 0 && encoder.start_count == 1,
            "MPP error/overflow/extra encoder");
    require(speech.xrun_count == 0 && speech.audio_overflow_count == 0, "Audio XRUN/overflow");
    require(vision.running() && image.state != infer::VisionRuntimeState::ERROR, "Vision worker failed");
    require(voice.state() != voice::VoiceRuntimeState::ERROR, "VoiceRuntime failed");
}
} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    try {
        const auto options = parse(argc,argv);
        std::ifstream cmdline("/proc/cmdline");
        std::string boot;
        std::getline(cmdline,boot);
        require(boot.find("amp_test_stage=C") != std::string::npos &&
                boot.find("mpu_sensor=MPU_SENSOR_COEXISTENCE_V1") != std::string::npos,
                "paired MPU sensor coexistence runtime required");
        HealthGate gate;
        gate.check(false);
        std::filesystem::create_directories(options.output_dir);
        QApplication app(argc,argv);
        ui::CoreIntegrationRuntimeOptions config;
        config.sensor_device_path="/dev/rk3576-sensor-v1";
        config.media_backend = ui::MediaBackendKind::Cam0Real;
        config.camera.device = options.camera;
        config.camera.camera_id = "front";
        config.camera.width = 1632;
        config.camera.height = 1224;
        config.camera.pixel_format = "NV12";
        config.camera.fps = 30;
        config.snapshot_directory = options.output_dir;
        config.recording_directory = options.output_dir;
        config.rtsp.port = 8554;
        config.rtsp.path = "/cam0";
        ui::CoreIntegrationRuntime runtime(config);
        require(runtime.start(),"Qt/Core foundation start");
        auto service = runtime.mediaService();
        require(service != nullptr,"one real MediaService missing");
        infer::SerialInferenceScheduler scheduler;
        infer::RknnVisionBackend backend(scheduler,{options.vision_model,"MobileNetV1 RK3576",5});
        auto vision = std::make_shared<infer::VisionRuntime>(backend,infer::VisionRuntimeConfig{8.0,2,114});
        auto window = std::make_unique<ui::MainWindow>(
            std::make_unique<ui::VisionUiBackend>(runtime.makeUiBackend(),vision),runtime.previewMailbox());
        window->setWindowTitle(QStringLiteral("SYSTEM COEXISTENCE TEST ONLY"));
        window->showFullScreen();
        window->showPage(ui::PageId::Camera);
        auto clock = std::make_shared<vehicle::SystemClock>();
        voice::SherpaAsrBackend asr(voice::make_sherpa_c_api_engine());
        voice::VoiceSessionController controller(runtime.client().boot_epoch());
        voice::DeterministicIntentRouter router;
        vehicle::VehicleCommandSinkAdapter sink(runtime.client(),clock,runtime.client().boot_epoch());
        audio::AlsaAudioCapture capture(options.audio);
        voice::SherpaVadBackend vad;
        voice::VoiceRuntimeConfig voice_config;
        voice_config.vad.model_path = options.vad_model;
        voice::VoiceRuntime speech(capture,vad,asr,controller,router,sink,
            [clock] { return clock->now_ms(); },voice_config,{},[&capture] { return capture.metrics().xrun_count; });
        std::atomic<bool> cancelled{false};
        std::atomic<int> result{1};
        std::thread worker([&] {
            protocol::RequestId next = 8000000;
            try {
                std::this_thread::sleep_for(2s);
                gate.check(true);
                wait_until([&] {return runtime.core().get_snapshot().sensor_state.data==vehicle::SensorDataCondition::VALID;},cancelled,10s,"real sensor");
                std::cout << "STAGE T2_QT_CORE PASS (sensor real runtime; control domain remains simulated)\n";
                command(runtime,vehicle::CommandType::CAMERA_PREVIEW_START,next,clock);
                wait_until([&] { return service->capture_stats().frames >= 90; },cancelled,10s,"CAM0 preview");
                gate.check(true);
                std::cout << "STAGE T3_CAM0_PREVIEW PASS\n";
                service->set_vision_frame_callback([vision](std::shared_ptr<const media::CapturedFrame> frame) {
                    if (!frame) return;
                    infer::VisionFrame input;
                    input.camera_id=frame->camera_id; input.width=frame->width; input.height=frame->height;
                    input.pixel_format=frame->pixel_format; input.bytes_per_line=frame->bytes_per_line;
                    input.bytes_used=frame->bytes_used; input.sequence=frame->sequence;
                    input.stream_epoch=frame->stream_epoch; input.capture_timestamp_ns=frame->capture_timestamp_ns;
                    input.dequeue_steady_timestamp_ns=frame->dequeue_steady_timestamp_ns;
                    const auto* payload=&frame->payload;
                    input.payload=std::shared_ptr<const std::vector<std::uint8_t>>(std::move(frame),payload);
                    (void)vision->submit(std::move(input));
                });
                require(vision->start().ok(),"RKNN start/load");
                media_operation(*service,media::MediaOperation::VisionStart);
                wait_until([&] { return vision->snapshot().metrics.vision_inferred_frames >= 16; },cancelled,10s,"RKNN frames");
                gate.check(true);
                std::cout << "STAGE T4_RKNN_VISION PASS\n";
                command(runtime,vehicle::CommandType::RTSP_START,next,clock);
                std::this_thread::sleep_for(2s);
                gate.check(true);
                std::cout << "STAGE T5_RTSP PASS url=rtsp://<actual-ip>:8554/cam0\n";
                command(runtime,vehicle::CommandType::RECORDING_START,next,clock);
                wait_until([&] { return service->recorder_stats().packets >= 60; },cancelled,10s,"Recording packets");
                gate.check(true);
                std::cout << "STAGE T6_RECORDING PASS\n";
                voice::SherpaModelFiles files;
                require(voice::read_sherpa_model_manifest(options.asr_config,options.asr_dir,files).ok(),"ASR manifest");
                require(asr.load(files).ok(),"ASR load");
                require(speech.start().ok(),"ALSA/VAD/Sherpa VoiceRuntime start");
                wait_until([&] { return capture.metrics().captured_frames >= 32000; },cancelled,10s,"ALSA frames");
                gate.check(true);
                std::cout << "STAGE T7_REAL_VOICE_RUNTIME PASS synthetic_FINAL=0\n";
                const auto started=std::chrono::steady_clock::now();
                auto next_sample=started+5s;
                while (std::chrono::steady_clock::now()-started < std::chrono::seconds(options.seconds)) {
                    require(!cancelled.load(),"UI closed before 300s");
                    require(runtime.core().get_snapshot().sensor_state.data==vehicle::SensorDataCondition::VALID,"sensor stream stopped during full load");
                    ui::require_coexistence_consumers(*service, std::cerr);
                    if (std::chrono::steady_clock::now()>=next_sample) {
                        gate.check(true);
                        sensor_metrics(runtime);
                        metrics(*service,*vision,speech,capture);
                        next_sample=std::chrono::steady_clock::now()+5s;
                    }
                    std::this_thread::sleep_for(100ms);
                }
                gate.check(false);
                sensor_metrics(runtime);
                metrics(*service,*vision,speech,capture);
                std::cout << "COEXISTENCE_DURATION_MS=" << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-started).count() << '\n';
                require(speech.stop().ok(),"VoiceRuntime stop");
                asr.unload();
                command(runtime,vehicle::CommandType::RECORDING_STOP,next,clock);
                command(runtime,vehicle::CommandType::RTSP_STOP,next,clock);
                media_operation(*service,media::MediaOperation::VisionStop);
                require(vision->stop().ok(),"Vision worker stop");
                service->set_vision_frame_callback({});
                command(runtime,vehicle::CommandType::CAMERA_PREVIEW_STOP,next,clock);
                const auto recording=service->recorder_stats();
                require(recording.file_closed && recording.output_bytes>0,"Recording not closed");
                require(!service->streaming(),"CAM0 still active after shutdown");
                runtime.stop();
                require(runtime.core().get_snapshot().sensor_state.unsubscribe_confirmed,"sensor UNSUB RESULT missing");
                std::this_thread::sleep_for(2s);
                gate.check(true);
                std::cout << "APPLICATION_SHUTDOWN_DONE recording_file=" << recording.output_path << '\n';
                result.store(0);
            } catch (const std::exception& failure) {
                std::cerr << "SYSTEM_COEXISTENCE_TEST_FAIL " << failure.what() << '\n';
                const auto state = runtime.core().get_snapshot();
                const auto voice_stats = speech.metrics();
                std::cerr << "COEXISTENCE_FAILURE_CONTEXT revision=" << state.revision
                          << " core_preview=" << static_cast<unsigned>(state.preview.value)
                          << " core_recording=" << static_cast<unsigned>(state.recording.value)
                          << " core_rtsp=" << static_cast<unsigned>(state.rtsp.value)
                          << " core_vision=" << static_cast<unsigned>(state.vision.value)
                          << " real_voice_finals=" << voice_stats.final_count
                          << " voice_intent_matches=" << voice_stats.intent_match_count
                          << " voice_rejected=" << voice_stats.rejected_count
                          << " audio_xrun=" << voice_stats.xrun_count
                          << " audio_overflow=" << voice_stats.audio_overflow_count << '\n';
                (void)speech.stop();
                asr.unload();
                runtime.stop();
                (void)vision->stop();
            }
            QMetaObject::invokeMethod(&app,"quit",Qt::QueuedConnection);
        });
        app.exec();
        cancelled.store(true);
        worker.join();
        const auto preview=window->previewStats();
        if(result.load()==0 && (!preview.converted_frames || !preview.delivered_frames)){std::cerr<<"QT preview did not deliver frames\n";result.store(1);}
        std::cout << "QT_PREVIEW converted=" << preview.converted_frames
                  << " delivered=" << preview.delivered_frames
                  << " fps=" << preview.displayed_fps << '\n';
        std::cout<<"SENSOR_GUI ui_merges="<<window->sensorMergeCount()<<" human_confirmation=USER_CONFIRMATION_PENDING\n";
        window.reset();
        runtime.stop();
        if (result.load()==0) std::cout << "SYSTEM_COEXISTENCE_APPLICATION_PROBE_PASS\n";
        return result.load();
    } catch (const std::exception& failure) {
        std::cerr << "SYSTEM_COEXISTENCE_PREFLIGHT_FAIL " << failure.what() << '\n';
        return 1;
    }
}
