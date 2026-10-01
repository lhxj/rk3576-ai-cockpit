#include "cockpit/audio/wav_reader.hpp"
#include "cockpit/voice/sherpa_asr.hpp"
#include "cockpit/voice/sherpa_vad.hpp"
#include "cockpit/voice/vad_utterance.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace {
void usage() {
    std::cout << "Usage: cockpit_vad_fixture_test --model-config FILE --model-dir DIR "
                 "--vad-model FILE --wav FILE [--repeat 1..200] [--expect-final 1..200]\n";
}
long long metric_kb(const char* path, const char* prefix) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) if (line.rfind(prefix, 0) == 0) {
        std::istringstream stream(line.substr(std::string(prefix).size()));
        long long value = -1;
        if (stream >> value) return value;
    }
    return -1;
}
void resource(const char* phase) {
    struct rusage use {};
    getrusage(RUSAGE_SELF, &use);
    std::cout << "RESOURCE_PHASE " << phase << " rss_kb="
              << metric_kb("/proc/self/status", "VmRSS:") << " pss_kb="
              << metric_kb("/proc/self/smaps_rollup", "Pss:") << " mem_available_kb="
              << metric_kb("/proc/meminfo", "MemAvailable:")
              << " peak_rss_kb=" << use.ru_maxrss
              << " cpu_user_ms=" << use.ru_utime.tv_sec * 1000 + use.ru_utime.tv_usec / 1000
              << " cpu_sys_ms=" << use.ru_stime.tv_sec * 1000 + use.ru_stime.tv_usec / 1000
              << " thermal_zone0_mc=" << metric_kb("/sys/class/thermal/thermal_zone0/temp", "")
              << std::endl;
}
bool number(const std::string& s, int& out) {
    try { std::size_t n = 0; const int v = std::stoi(s, &n);
          if (n != s.size() || v < 1 || v > 200) return false;
          out = v; return true; }
    catch (...) { return false; }
}
}

int main(int argc, char** argv) {
    std::string manifest, model_dir, vad_model, wav_path;
    int repeat = 1, expected = 1;
    for (int i = 1; i < argc;) {
        const std::string key = argv[i];
        if (key == "--help") { usage(); return 0; }
        if (i + 1 >= argc) { usage(); return 2; }
        const std::string value = argv[i + 1];
        if (key == "--model-config") manifest = value;
        else if (key == "--model-dir") model_dir = value;
        else if (key == "--vad-model") vad_model = value;
        else if (key == "--wav") wav_path = value;
        else if (key == "--repeat") { if (!number(value, repeat)) return 2; }
        else if (key == "--expect-final") { if (!number(value, expected)) return 2; }
        else { usage(); return 2; }
        i += 2;
    }
    if (manifest.empty() || model_dir.empty() || vad_model.empty() || wav_path.empty()) {
        usage(); return 2;
    }
    cockpit::voice::SherpaModelFiles files;
    auto status = cockpit::voice::read_sherpa_model_manifest(manifest, model_dir, files);
    if (!status.ok()) { std::cerr << "MODEL_ERROR " << status.detail << '\n'; return 3; }
    auto audio = cockpit::audio::read_pcm_wav(wav_path);
    if (!audio.status.ok()) { std::cerr << "WAV_ERROR " << audio.status.detail << '\n'; return 4; }
    cockpit::voice::SherpaAsrBackend asr(cockpit::voice::make_sherpa_c_api_engine());
    cockpit::voice::SherpaVadBackend vad;
    cockpit::voice::VadConfig config;
    config.model_path = vad_model;
    resource("before_load");
    status = asr.load(files);
    if (!status.ok()) { std::cerr << "MODEL_ERROR " << status.detail << '\n'; return 5; }
    std::cout << "MODEL_LOADED asr=1" << std::endl;
    resource("after_asr_load");
    cockpit::voice::VoiceSessionController controller(1);
    int finals = 0;
    long long sampled_peak_pss_kb = 0;
    long long sampled_min_mem_available_kb = 1LL << 60;
    long long sampled_peak_thermal_mc = -1;
    cockpit::voice::VadUtteranceProcessor processor(vad, asr, controller,
        [&](const cockpit::voice::AsrEvent& e) {
            if (e.type == cockpit::voice::AsrEventType::FINAL) {
                ++finals;
                std::cout << "ASR_FINAL session=" << e.token.session_id << " " << e.text << std::endl;
                if (finals % 10 == 0) {
                    sampled_peak_pss_kb = std::max(sampled_peak_pss_kb,
                        metric_kb("/proc/self/smaps_rollup", "Pss:"));
                    const auto available = metric_kb("/proc/meminfo", "MemAvailable:");
                    if (available >= 0)
                        sampled_min_mem_available_kb = std::min(sampled_min_mem_available_kb, available);
                    sampled_peak_thermal_mc = std::max(sampled_peak_thermal_mc,
                        metric_kb("/sys/class/thermal/thermal_zone0/temp", ""));
                }
            } else if (e.type == cockpit::voice::AsrEventType::ERROR)
                std::cerr << "ASR_ERROR " << e.status.detail << std::endl;
        }, [&](const cockpit::voice::VadEvent& e) {
            std::cout << (e.type == cockpit::voice::VadEventType::SpeechStarted ?
                "VAD_SPEECH_STARTED" : "VAD_SPEECH_ENDED")
                << " sample_index=" << e.sample_index << std::endl;
        });
    status = processor.configure(config);
    if (!status.ok()) { std::cerr << "VAD_ERROR " << status.detail << '\n'; return 6; }
    std::cout << "VAD_BACKEND sherpa-silero-v1.11.3 threshold=" << config.threshold
              << " min_speech_ms=" << config.min_speech_ms
              << " min_silence_ms=" << config.min_silence_ms
              << " pre_roll_ms=" << config.pre_roll_ms
              << " max_utterance_ms=" << config.max_utterance_ms
              << " sample_rate=" << config.sample_rate << std::endl;
    resource("after_vad_load");
    std::uint64_t sequence = 1;
    auto feed = [&](const std::vector<std::uint8_t>& bytes) {
        constexpr std::size_t kChunkBytes = 640;
        for (std::size_t at = 0; at < bytes.size(); at += kChunkBytes) {
            const auto end = std::min(bytes.size(), at + kChunkBytes);
            cockpit::audio::PcmChunk chunk;
            chunk.pcm.format = audio.pcm.format;
            chunk.pcm.bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                                   bytes.begin() + static_cast<std::ptrdiff_t>(end));
            chunk.frames = chunk.pcm.bytes.size() / 2;
            chunk.sequence = sequence++;
            chunk.captured_at = std::chrono::steady_clock::now();
            auto s = processor.accept(chunk);
            if (!s.ok()) { std::cerr << "PIPELINE_ERROR " << s.detail << '\n'; return false; }
        }
        return true;
    };
    const std::vector<std::uint8_t> one_second_silence(16000 * 2, 0);
    for (int i = 0; i < repeat; ++i) {
        if (!feed(one_second_silence) || !feed(audio.pcm.bytes) ||
            !feed(one_second_silence)) return 7;
    }
    status = processor.flush();
    if (!status.ok()) { std::cerr << "PIPELINE_ERROR " << status.detail << '\n'; return 8; }
    const auto m = processor.metrics();
    std::cout << "VAD_FIXTURE_METRICS starts=" << m.vad_speech_start_count
              << " ends=" << m.vad_speech_end_count << " utterances=" << m.utterance_count
              << " finals=" << finals << " asr_model_loads=1 vad_model_loads=" << vad.load_count()
              << " pre_roll_frames=" << m.pre_roll_frames
              << " peak_pre_roll_frames=" << m.peak_pre_roll_frames
              << " sequence_gaps=" << m.pcm_sequence_gap_count
              << " vad_processing_ms=" << m.vad_processing_ms
              << " sampled_peak_pss_kb=" << sampled_peak_pss_kb
              << " sampled_min_mem_available_kb=" <<
                  (sampled_min_mem_available_kb == (1LL << 60) ? -1 : sampled_min_mem_available_kb)
              << " sampled_peak_thermal_mc=" << sampled_peak_thermal_mc << std::endl;
    resource("after_fixture");
    processor.stop();
    asr.unload();
    return finals == expected && m.utterance_count == static_cast<std::uint64_t>(expected)
        && vad.load_count() == 1 ? 0 : 9;
}
