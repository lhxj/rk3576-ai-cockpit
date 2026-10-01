#include "cockpit/audio/wav_reader.hpp"
#include "cockpit/voice/sherpa_asr.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace {
void usage() {
    std::cout << "Usage: cockpit_asr_file_test --model-config FILE --model-dir DIR [--load-only | --wav FILE [--wav FILE ...]] [--expect-contains TEXT]\n";
}
long long metric_kb(const char* path, const std::string& name) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind(name, 0) == 0) {
            std::istringstream value(line.substr(name.size()));
            long long kb = -1;
            if (value >> kb) return kb;
        }
    }
    return -1;
}
void resource_phase(const char* phase) {
    struct rusage use {};
    const long long peak_rss = getrusage(RUSAGE_SELF, &use) == 0 ? use.ru_maxrss : -1;
    std::cout << "RESOURCE_PHASE " << phase
              << " rss_kb=" << metric_kb("/proc/self/status", "VmRSS:")
              << " pss_kb=" << metric_kb("/proc/self/smaps_rollup", "Pss:")
              << " peak_rss_kb=" << peak_rss
              << " mem_available_kb=" << metric_kb("/proc/meminfo", "MemAvailable:") << '\n';
}
}  // namespace

int main(int argc, char** argv) {
    std::string manifest, model_dir, expected;
    std::vector<std::string> wav_paths;
    bool load_only = false;
    for (int i = 1; i < argc;) {
        const std::string key = argv[i];
        if (key == "--help") { usage(); return 0; }
        if (key == "--load-only") { load_only = true; ++i; continue; }
        if (i + 1 >= argc) { usage(); return 2; }
        if (key == "--model-config") manifest = argv[i + 1];
        else if (key == "--model-dir") model_dir = argv[i + 1];
        else if (key == "--wav") wav_paths.emplace_back(argv[i + 1]);
        else if (key == "--expect-contains") expected = argv[i + 1];
        else { usage(); return 2; }
        i += 2;
    }
    if (manifest.empty() || model_dir.empty() || (wav_paths.empty() && !load_only) ||
        (load_only && !wav_paths.empty())) { usage(); return 2; }
    cockpit::voice::SherpaModelFiles files;
    auto status = cockpit::voice::read_sherpa_model_manifest(manifest, model_dir, files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 3; }
    cockpit::voice::SherpaAsrBackend backend(cockpit::voice::make_sherpa_c_api_engine());
    resource_phase("before_load");
    const auto load_begin = std::chrono::steady_clock::now();
    status = backend.load(files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 4; }
    const double load_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - load_begin).count();
    std::cout << "MODEL_LOADED load_ms=" << load_ms << '\n';
    resource_phase("after_load");
    if (load_only) return 0;
    cockpit::voice::VoiceSessionController controller(1);
    for (std::size_t index = 0; index < wav_paths.size(); ++index) {
        auto audio = cockpit::audio::read_pcm_wav(wav_paths[index]);
        if (!audio.status.ok()) { std::cerr << audio.status.detail << '\n'; return 5; }
        std::cout << "AUDIO_INFO sample_rate=" << audio.pcm.format.sample_rate
                  << " channels=" << audio.pcm.format.channels << " format=S16_LE duration_s="
                  << audio.duration_seconds << '\n';
        const auto token = controller.start(index + 1);
        audio.pcm.session_id = token.session_id;
        status = controller.transition(token, cockpit::voice::VoiceSessionState::Recognizing);
        if (!status.ok()) { std::cerr << status.detail << '\n'; return 6; }
        bool final_seen = false;
        std::string final_text;
        const auto begin = std::chrono::steady_clock::now();
        status = backend.start_session(token, audio.pcm.format, [&](const cockpit::voice::AsrEvent& event) {
            cockpit::protocol::MessageType message_type = cockpit::protocol::MessageType::ASR_ERROR;
            if (event.type == cockpit::voice::AsrEventType::PARTIAL) message_type = cockpit::protocol::MessageType::ASR_PARTIAL;
            if (event.type == cockpit::voice::AsrEventType::FINAL) message_type = cockpit::protocol::MessageType::ASR_FINAL;
            controller.deliver_event(event.token, message_type, [&] {
                if (event.type == cockpit::voice::AsrEventType::PARTIAL) std::cout << "ASR_PARTIAL " << event.text << '\n';
                else if (event.type == cockpit::voice::AsrEventType::FINAL) {
                    final_seen = !event.text.empty();
                    final_text = event.text;
                    std::cout << "ASR_FINAL " << event.text << '\n';
                } else std::cerr << "ASR_ERROR " << event.status.detail << '\n';
            });
        });
        if (!status.ok()) { std::cerr << status.detail << '\n'; return 6; }
        status = backend.push_audio(audio.pcm);
        if (status.ok()) status = backend.finish_input(token);
        if (!status.ok()) { std::cerr << status.detail << '\n'; return 7; }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::cout << "FILE_ASR_METRICS decode_s=" << elapsed << " rtf="
                  << elapsed / audio.duration_seconds << '\n';
        resource_phase("after_decode");
        if (!final_seen) return 8;
        if (!expected.empty() && final_text.find(expected) == std::string::npos) return 9;
    }
    return 0;
}
