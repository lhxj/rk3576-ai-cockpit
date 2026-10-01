#include "cockpit/audio/wav_reader.hpp"
#include "cockpit/voice/live_asr_pipeline.hpp"
#include "cockpit/voice/sherpa_asr.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

using namespace cockpit;
using namespace std::chrono_literals;
namespace {
// Test-only paced capture. It exercises the unchanged LiveAsrPipeline with
// the real recognizer while never opening or emulating an ALSA device.
class FileCapture final : public audio::IAudioCapture {
public:
    explicit FileCapture(audio::PcmBuffer pcm) : pcm_(std::move(pcm)) {}
    protocol::Status start(const audio::AudioFormat&) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return {protocol::StatusCode::INVALID_STATE, "capture already active"};
        running_ = true;
        return protocol::Status::Ok();
    }
    audio::AudioCaptureResult read(std::chrono::milliseconds) override {
        std::this_thread::sleep_for(20ms);
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return {{protocol::StatusCode::CANCELLED, "capture stopped"}, {}};
        if (offset_ >= pcm_.bytes.size()) return {{protocol::StatusCode::TIMEOUT, "end of fixture"}, {}};
        const auto size = std::min<std::size_t>(640, pcm_.bytes.size() - offset_);
        audio::PcmBuffer out;
        out.format = pcm_.format;
        out.bytes.assign(pcm_.bytes.begin() + static_cast<std::ptrdiff_t>(offset_),
                         pcm_.bytes.begin() + static_cast<std::ptrdiff_t>(offset_ + size));
        offset_ += size;
        delivered_ = offset_;
        return {protocol::Status::Ok(), std::move(out)};
    }
    void stop() override { std::lock_guard<std::mutex> lock(mutex_); running_ = false; }
    audio::AudioDeviceState state() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_ ? audio::AudioDeviceState::RUNNING : audio::AudioDeviceState::STOPPED;
    }
    std::optional<audio::AudioFormat> actual_format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return std::nullopt;
        return pcm_.format;
    }
    std::size_t delivered() const { return delivered_; }
    std::size_t total() const { return pcm_.bytes.size(); }
private:
    audio::PcmBuffer pcm_;
    mutable std::mutex mutex_;
    bool running_{false};
    std::size_t offset_{0};
    std::atomic<std::size_t> delivered_{0};
};
}

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    voice::SherpaModelFiles files;
    auto status = voice::read_sherpa_model_manifest(argv[1], argv[2], files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 3; }
    auto wav = audio::read_pcm_wav(argv[3]);
    if (!wav.status.ok()) { std::cerr << wav.status.detail << '\n'; return 4; }
    FileCapture capture(std::move(wav.pcm));
    voice::SherpaAsrBackend asr(voice::make_sherpa_c_api_engine());
    status = asr.load(files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 5; }
    voice::VoiceSessionController controller(1);
    int finals = 0;
    voice::LiveAsrPipeline pipeline(capture, asr, controller, [&](const voice::AsrEvent& event) {
        if (event.type == voice::AsrEventType::FINAL) {
            ++finals;
            std::cout << "FIXED_LIVE_ASR_FINAL " << event.text << std::endl;
        }
    });
    status = pipeline.start(1);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 6; }
    const auto until = std::chrono::steady_clock::now() + 60s;
    while (capture.delivered() < capture.total() && !pipeline.failed() &&
           std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(10ms);
    if (capture.delivered() != capture.total()) { pipeline.cancel(); return 7; }
    status = pipeline.finish();
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 8; }
    const auto m = pipeline.metrics();
    std::cout << "FIXED_LIVE_FILE_METRICS finals=" << finals
              << " queue_overflow=" << m.queue_overflow_count
              << " peak_queue=" << m.max_queue_depth << std::endl;
    asr.unload();
    return finals == 1 && m.queue_overflow_count == 0 ? 0 : 9;
}
