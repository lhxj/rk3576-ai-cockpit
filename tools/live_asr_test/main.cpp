#include "cockpit/audio/alsa_capture.hpp"
#include "cockpit/voice/live_asr_pipeline.hpp"
#include "cockpit/voice/sherpa_asr.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using namespace cockpit;
using namespace std::chrono_literals;
namespace {
void usage() {
    std::cout << "Usage: cockpit_live_asr_test [--device PCM] [--duration 1..30] "
                 "[--probe | --capture-only | --model-config FILE --model-dir DIR "
                 "[--session-count 1..3] [--cancel-after 1..29]]\n";
}
bool parse_positive(const std::string& value, int& out, int high) {
    try {
        std::size_t read = 0;
        const int parsed = std::stoi(value, &read);
        if (read != value.size() || parsed < 1 || parsed > high) return false;
        out = parsed;
        return true;
    } catch (...) { return false; }
}
long long metric_kb(const char* path, const char* prefix) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind(prefix, 0) == 0) {
            std::istringstream stream(line.substr(std::string(prefix).size()));
            long long value = -1;
            if (stream >> value) return value;
        }
    }
    return -1;
}
void resource(const char* phase) {
    std::cout << "RESOURCE_PHASE " << phase
              << " rss_kb=" << metric_kb("/proc/self/status", "VmRSS:")
              << " pss_kb=" << metric_kb("/proc/self/smaps_rollup", "Pss:")
              << " mem_available_kb=" << metric_kb("/proc/meminfo", "MemAvailable:") << std::endl;
}
void audio_format(const audio::AlsaAudioCapture& capture) {
    const auto format = capture.actual_format();
    const auto counters = capture.metrics();
    if (!format) return;
    std::cout << "AUDIO_DEVICE_OPENED\nAUDIO_FORMAT sample_rate=" << format->sample_rate
              << " channels=" << format->channels << " format=S16_LE period_frames="
              << counters.period_frames << " buffer_frames=" << counters.buffer_frames
              << " period_ms=" << 1000.0 * counters.period_frames / format->sample_rate
              << " buffer_ms=" << 1000.0 * counters.buffer_frames / format->sample_rate << std::endl;
}
double rms_level(const audio::AlsaCaptureMetrics& counters) {
    if (counters.captured_frames == 0) return 0;
    return std::sqrt(static_cast<double>(counters.sample_square_sum) /
                     static_cast<double>(counters.captured_frames));
}
}  // namespace

int main(int argc, char** argv) {
    std::string device = "hw:0,0", manifest, model_dir;
    int duration = 5, sessions = 1, cancel_after = 0;
    bool probe = false, capture_only = false;
    for (int i = 1; i < argc;) {
        const std::string key = argv[i];
        if (key == "--help") { usage(); return 0; }
        if (key == "--probe") { probe = true; ++i; continue; }
        if (key == "--capture-only") { capture_only = true; ++i; continue; }
        if (i + 1 >= argc) { usage(); return 2; }
        const std::string value = argv[i + 1];
        if (key == "--device") device = value;
        else if (key == "--model-config") manifest = value;
        else if (key == "--model-dir") model_dir = value;
        else if (key == "--duration") {
            if (!parse_positive(value, duration, 30)) { usage(); return 2; }
        } else if (key == "--session-count") {
            if (!parse_positive(value, sessions, 3)) { usage(); return 2; }
        } else if (key == "--cancel-after") {
            if (!parse_positive(value, cancel_after, 29)) { usage(); return 2; }
        } else { usage(); return 2; }
        i += 2;
    }
    if (device.empty() || (probe && capture_only) ||
        (!probe && !capture_only && (manifest.empty() || model_dir.empty())) ||
        (cancel_after && (probe || capture_only || cancel_after >= duration))) {
        usage(); return 2;
    }
    audio::AlsaAudioCapture capture(device);
    if (probe || capture_only) {
        auto status = capture.start({});
        if (!status.ok()) { std::cerr << "AUDIO_ERROR " << status.detail << std::endl; return 3; }
        audio_format(capture);
        if (probe) { capture.stop(); std::cout << "AUDIO_DEVICE_CLOSED" << std::endl; return 0; }
        const auto begin = std::chrono::steady_clock::now();
        const auto until = begin + std::chrono::seconds(duration);
        while (std::chrono::steady_clock::now() < until) {
            auto result = capture.read(100ms);
            if (result.status.code == protocol::StatusCode::TIMEOUT) continue;
            if (!result.status.ok()) {
                std::cerr << "AUDIO_ERROR " << result.status.detail << std::endl;
                capture.stop(); return 4;
            }
        }
        const auto counters = capture.metrics();
        capture.stop();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << "AUDIO_CAPTURE_METRICS duration_s=" << seconds
                  << " captured_frames=" << counters.captured_frames
                  << " xrun_count=" << counters.xrun_count
                  << " peak_abs_sample=" << counters.peak_abs_sample
                  << " rms_sample=" << rms_level(counters)
                  << " nonzero_samples=" << counters.nonzero_samples << std::endl;
        return counters.captured_frames > 0 && counters.xrun_count == 0 ? 0 : 5;
    }
    voice::SherpaModelFiles files;
    auto status = voice::read_sherpa_model_manifest(manifest, model_dir, files);
    if (!status.ok()) { std::cerr << "MODEL_ERROR " << status.detail << std::endl; return 6; }
    voice::SherpaAsrBackend backend(voice::make_sherpa_c_api_engine());
    resource("before_load");
    const auto load_begin = std::chrono::steady_clock::now();
    status = backend.load(files);
    if (!status.ok()) { std::cerr << "MODEL_ERROR " << status.detail << std::endl; return 7; }
    std::cout << "MODEL_LOADED load_ms=" << std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - load_begin).count() << std::endl;
    resource("after_load");
    voice::VoiceSessionController controller(1);
    bool have_final = false;
    for (int index = 0; index < sessions; ++index) {
        have_final = false;
        voice::LiveAsrPipeline pipeline(capture, backend, controller, [&](const voice::AsrEvent& event) {
            if (event.type == voice::AsrEventType::PARTIAL)
                std::cout << "ASR_PARTIAL session=" << event.token.session_id << " " << event.text << std::endl;
            else if (event.type == voice::AsrEventType::FINAL) {
                have_final = !event.text.empty();
                std::cout << "ASR_FINAL session=" << event.token.session_id << " " << event.text << std::endl;
            } else std::cerr << "ASR_ERROR session=" << event.token.session_id << " "
                             << event.status.detail << std::endl;
        });
        status = pipeline.start(static_cast<protocol::RequestId>(index + 1));
        if (!status.ok()) { std::cerr << "SESSION_ERROR " << status.detail << std::endl; return 8; }
        audio_format(capture);
        std::cout << "SESSION_STARTED id=" << pipeline.token().session_id
                  << " speak_now=1 duration_s=" << (index == 0 && cancel_after ? cancel_after : duration)
                  << std::endl;
        const auto run_begin = std::chrono::steady_clock::now();
        const auto hold = index == 0 && cancel_after ? cancel_after : duration;
        while (std::chrono::steady_clock::now() - run_begin < std::chrono::seconds(hold) && !pipeline.failed())
            std::this_thread::sleep_for(20ms);
        const auto cancel_begin = std::chrono::steady_clock::now();
        if (index == 0 && cancel_after) status = pipeline.cancel();
        else status = pipeline.finish();
        const double stop_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - cancel_begin).count();
        const auto metrics = pipeline.metrics();
        const auto counters = capture.metrics();
        std::cout << "METRICS session=" << pipeline.token().session_id
                  << " captured_frames=" << metrics.captured_frames
                  << " period_frames=" << counters.period_frames
                  << " queue_capacity=" << metrics.queue_capacity
                  << " max_queue_depth=" << metrics.max_queue_depth
                  << " queue_overflow_count=" << metrics.queue_overflow_count
                  << " xrun_count=" << counters.xrun_count
                  << " peak_abs_sample=" << counters.peak_abs_sample
                  << " rms_sample=" << rms_level(counters)
                  << " nonzero_samples=" << counters.nonzero_samples
                  << " partial_count=" << metrics.partial_count
                  << " first_partial_ms=" << metrics.first_partial_ms
                  << " final_after_capture_stop_ms=" << metrics.final_after_capture_stop_ms
                  << " capture_stop_ms=" << metrics.capture_stop_ms
                  << " session_stop_ms=" << metrics.session_stop_ms
                  << " stop_ms=" << stop_ms << std::endl;
        resource("after_session");
        if (!status.ok()) { std::cerr << "SESSION_ERROR " << status.detail << std::endl; return 9; }
        if (index == 0 && cancel_after) {
            if (have_final) { std::cerr << "CANCEL_OLD_FINAL_ERROR" << std::endl; return 10; }
            std::cout << "SESSION_CANCELLED id=" << pipeline.token().session_id << std::endl;
        } else {
            if (!have_final) { std::cerr << "EMPTY_FINAL" << std::endl; return 11; }
            std::cout << "SESSION_COMPLETED id=" << pipeline.token().session_id << std::endl;
        }
    }
    backend.unload();
    std::cout << "LIVE_ASR_TEST_EXIT_OK" << std::endl;
    return 0;
}
