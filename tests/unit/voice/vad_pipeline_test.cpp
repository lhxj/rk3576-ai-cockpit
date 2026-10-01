#include "cockpit/voice/vad_live_pipeline.hpp"

#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace cockpit;
using namespace std::chrono_literals;
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

audio::PcmChunk chunk(std::uint64_t sequence, std::int16_t sample, std::size_t frames = 320) {
    audio::PcmChunk c;
    c.sequence = sequence;
    c.captured_at = std::chrono::steady_clock::now();
    c.frames = frames;
    c.pcm.format = {};
    c.pcm.bytes.reserve(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        c.pcm.bytes.push_back(static_cast<std::uint8_t>(sample));
        c.pcm.bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(sample) >> 8));
    }
    return c;
}

// Deterministic test double. It models only the documented min-speech/min-silence
// event contract, not Silero's probability or speech accuracy.
class FixtureVad final : public voice::IVadBackend {
public:
    protocol::Status configure(const voice::VadConfig& c) override {
        cfg = c; ++loads; reset(); return protocol::Status::Ok();
    }
    void reset() override { speaking = false; hot_frames = 0; quiet_frames = 0; position = 0; }
    voice::VadAcceptResult accept_samples(const audio::PcmBuffer& pcm,
            std::chrono::steady_clock::time_point at) override {
        if (slow) std::this_thread::sleep_for(4ms);
        const auto raw = static_cast<std::uint16_t>(pcm.bytes[0] | (pcm.bytes[1] << 8));
        const auto signed_sample = raw < 32768 ? static_cast<int>(raw) : static_cast<int>(raw) - 65536;
        const bool hot = signed_sample > 1000 || signed_sample < -1000;
        const auto frames = pcm.bytes.size() / 2;
        position += frames;
        voice::VadAcceptResult r{protocol::Status::Ok(), {}};
        if (hot) {
            quiet_frames = 0;
            hot_frames += frames;
            if (!speaking && hot_frames >= cfg.sample_rate * cfg.min_speech_ms / 1000) {
                speaking = true;
                r.events.push_back({voice::VadEventType::SpeechStarted, position, at, -1});
            }
        } else if (speaking) {
            quiet_frames += frames;
            if (quiet_frames >= cfg.sample_rate * cfg.min_silence_ms / 1000) {
                speaking = false;
                hot_frames = quiet_frames = 0;
                r.events.push_back({voice::VadEventType::SpeechEnded, position, at, -1});
            }
        } else hot_frames = 0;
        return r;
    }
    voice::VadAcceptResult flush() override {
        voice::VadAcceptResult r{protocol::Status::Ok(), {}};
        if (speaking) r.events.push_back({voice::VadEventType::SpeechEnded, position,
                                         std::chrono::steady_clock::now(), -1});
        speaking = false;
        return r;
    }
    voice::VadState state() const override {
        return speaking ? voice::VadState::SpeechActive : voice::VadState::Silence;
    }
    voice::VadConfig cfg;
    bool speaking{false};
    bool slow{false};
    int loads{0};
    std::size_t hot_frames{0}, quiet_frames{0};
    std::uint64_t position{0};
};

class FakeAsr final : public voice::IAsrBackend {
public:
    protocol::Status start_session(voice::SessionToken token, const audio::AudioFormat&,
                                   voice::AsrCallback cb) override {
        require(!active, "ASR stream overlap");
        active = true; current = token; callback = std::move(cb); ++starts;
        current_samples.clear(); return protocol::Status::Ok();
    }
    protocol::Status push_audio(const audio::PcmBuffer& pcm) override {
        require(active && pcm.session_id == current.session_id, "ASR token mismatch");
        for (std::size_t i = 0; i < pcm.bytes.size(); i += 2)
            current_samples.push_back(static_cast<std::int16_t>(
                static_cast<std::uint16_t>(pcm.bytes[i] | (pcm.bytes[i + 1] << 8))));
        return protocol::Status::Ok();
    }
    protocol::Status finish_input(voice::SessionToken token) override {
        require(active && token.session_id == current.session_id, "ASR finish mismatch");
        ++finals; all_samples.push_back(current_samples);
        callback({voice::AsrEventType::FINAL, token, 1, "fixture", protocol::Status::Ok()});
        active = false; return protocol::Status::Ok();
    }
    void cancel(voice::SessionToken token) override {
        if (active && token.session_id == current.session_id) { ++cancels; active = false; }
    }
    bool active{false};
    int starts{0}, finals{0}, cancels{0};
    voice::SessionToken current;
    voice::AsrCallback callback;
    std::vector<std::int16_t> current_samples;
    std::vector<std::vector<std::int16_t>> all_samples;
};

struct Rig {
    FixtureVad vad;
    FakeAsr asr;
    voice::VoiceSessionController controller{1};
    std::vector<voice::AsrEvent> events;
    voice::VadUtteranceProcessor processor{vad, asr, controller,
        [this](const voice::AsrEvent& e) { events.push_back(e); }};
    voice::VadConfig config;
    std::uint64_t seq{1};
    Rig() { require(processor.configure(config).ok(), "configure"); }
    void send(int chunks, std::int16_t value) {
        for (int i = 0; i < chunks; ++i)
            require(processor.accept(chunk(seq++, value)).ok(), "fixture accept");
    }
};

void segmentation() {
    { Rig r; r.send(100, 0); require(r.asr.starts == 0, "silence trigger"); }
    { Rig r; r.send(30, 100); require(r.asr.starts == 0, "low noise trigger"); }
    { Rig r; r.send(50, 0); r.send(5, 5000); r.send(50, 0);
      require(r.asr.starts == 0, "short burst trigger"); }
    { Rig r; r.send(50, 0); r.send(30, 5000); r.send(30, 0);
      require(r.asr.starts == 1 && r.asr.finals == 1, "one utterance");
      require(r.processor.metrics().vad_speech_end_count == 1, "one speech end"); }
    { Rig r; r.send(50, 0); r.send(30, 5000); r.send(30, 0);
      r.send(30, 5000); r.send(30, 0);
      require(r.asr.starts == 2 && r.asr.finals == 2, "two utterances");
      require(r.asr.all_samples[0] != r.asr.all_samples[1] ||
          r.controller.current().session_id == 2, "new ASR stream per utterance"); }
    { Rig r; r.send(30, 5000); r.send(10, 0); r.send(30, 5000); r.send(30, 0);
      require(r.asr.starts == 1 && r.asr.finals == 1, "short pause split"); }
}

void preroll_max_cancel() {
    { Rig r; r.send(50, 0); r.send(1, 1234); r.send(29, 5000); r.send(30, 0);
      require(r.asr.finals == 1, "pre-roll final");
      const auto& samples = r.asr.all_samples[0];
      require(std::find(samples.begin(), samples.end(), 1234) != samples.end(),
              "speech onset missing from ASR pre-roll");
      require(r.processor.metrics().peak_pre_roll_frames <= 4800, "pre-roll unbounded"); }
    { Rig r; r.config.max_utterance_ms = 1000; // configure a separate processor below
      FixtureVad vad; FakeAsr asr; voice::VoiceSessionController ctl(1);
      voice::VadUtteranceProcessor p(vad, asr, ctl, [](const voice::AsrEvent&){});
      require(p.configure(r.config).ok(), "max config");
      for (int i = 1; i <= 60; ++i) require(p.accept(chunk(i, 5000)).ok(), "max feed");
      require(p.metrics().forced_max_duration_count >= 1, "max duration not forced"); }
    { Rig r; r.send(20, 5000); require(r.asr.starts == 1, "cancel start");
      require(r.processor.cancel_current().ok(), "cancel request");
      r.send(1, 5000); r.send(30, 0);
      require(r.asr.finals == 0 && r.asr.cancels == 1, "cancel leaked FINAL");
      require(r.processor.state() == voice::UtteranceState::Listening, "cancel not Listening");
      r.send(30, 5000); r.send(30, 0);
      require(r.asr.finals == 1 && r.asr.starts == 2, "cancel restart"); }
    { Rig r; r.send(2, 0); auto gap = r.processor.accept(chunk(4, 0));
      require(!gap.ok() && r.processor.metrics().pcm_sequence_gap_count == 1,
              "sequence gap accepted"); }
}

void stress() {
    Rig r;
    for (int i = 0; i < 100; ++i) { r.send(15, 5000); r.send(25, 0); }
    require(r.asr.starts == 100 && r.asr.finals == 100, "100 utterance count");
    require(r.vad.loads == 1, "VAD model reloaded");
    require(r.processor.metrics().peak_pre_roll_frames <= 4800, "ring growth");
    r.processor.stop();
    require(r.processor.pre_roll_size_frames() == 0, "ring not cleared");
}

void bounded_live() {
    audio::MockAudioCapture capture(1000);
    FixtureVad vad; vad.slow = true;
    FakeAsr asr; voice::VoiceSessionController controller(1);
    voice::VadLivePipeline pipeline(capture, vad, asr, controller,
        [](const voice::AsrEvent&){}, {}, 1);
    require(pipeline.start({}).ok(), "mock live start");
    for (int i = 0; i < 300; ++i) {
        auto c = chunk(i + 1, 0);
        if (!capture.push_fixture(std::move(c.pcm)).ok()) break;
        if (pipeline.failed()) break;
    }
    std::this_thread::sleep_for(50ms);
    auto status = pipeline.stop();
    require(!status.ok() && pipeline.metrics().queue_overflow_count > 0,
            "live PCM overflow silently dropped");
    require(capture.state() == audio::AudioDeviceState::STOPPED, "capture leaked");
}
void normal_live_shutdown() {
    audio::MockAudioCapture capture(200);
    FixtureVad vad;
    FakeAsr asr; voice::VoiceSessionController controller(1);
    std::atomic<int> finals{0};
    voice::VadLivePipeline pipeline(capture, vad, asr, controller,
        [&](const voice::AsrEvent& e) {
            if (e.type == voice::AsrEventType::FINAL) ++finals;
        });
    require(pipeline.start({}).ok(), "normal live start");
    std::uint64_t sequence = 1;
    for (int run = 0; run < 2; ++run) {
        for (int i = 0; i < 30; ++i) {
            auto c = chunk(sequence++, 5000);
            require(capture.push_fixture(std::move(c.pcm)).ok(), "mock speech input");
            std::this_thread::sleep_for(100us);
        }
        for (int i = 0; i < 30; ++i) {
            auto c = chunk(sequence++, 0);
            require(capture.push_fixture(std::move(c.pcm)).ok(), "mock silence input");
            std::this_thread::sleep_for(100us);
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (finals.load() < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(2ms);
    require(finals == 2, "mock live auto utterances");
    require(pipeline.stop().ok(), "normal live stop");
    require(capture.state() == audio::AudioDeviceState::STOPPED, "normal capture shutdown");
    require(pipeline.metrics().queue_overflow_count == 0, "normal queue overflow");
    require(vad.loads == 1 && asr.starts == 2, "model/session lifecycle");
}
void live_cancel_keeps_capture() {
    audio::MockAudioCapture capture(200);
    FixtureVad vad;
    FakeAsr asr; voice::VoiceSessionController controller(1);
    std::atomic<int> finals{0};
    voice::VadLivePipeline pipeline(capture, vad, asr, controller,
        [&](const voice::AsrEvent& e) {
            if (e.type == voice::AsrEventType::FINAL) ++finals;
        });
    require(pipeline.start({}).ok(), "cancel live start");
    auto push = [&](int n, std::int16_t sample) {
        for (int i = 0; i < n; ++i) {
            auto c = chunk(1, sample);
            require(capture.push_fixture(std::move(c.pcm)).ok(), "cancel mock input");
            std::this_thread::sleep_for(100us);
        }
    };
    push(20, 5000);
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (pipeline.utterance_state() != voice::UtteranceState::Speaking &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    require(pipeline.cancel_current().ok(), "live utterance cancel");
    require(capture.state() == audio::AudioDeviceState::RUNNING,
            "cancel closed audio device");
    push(30, 0);
    const auto reset_deadline = std::chrono::steady_clock::now() + 2s;
    while (pipeline.utterance_state() != voice::UtteranceState::Listening &&
           std::chrono::steady_clock::now() < reset_deadline)
        std::this_thread::sleep_for(1ms);
    require(pipeline.utterance_state() == voice::UtteranceState::Listening,
            "cancel did not return to Listening");
    push(30, 5000); push(30, 0);
    const auto final_deadline = std::chrono::steady_clock::now() + 2s;
    while (finals.load() < 1 && std::chrono::steady_clock::now() < final_deadline)
        std::this_thread::sleep_for(1ms);
    require(finals == 1, "new utterance after live cancel");
    require(pipeline.stop().ok(), "cancel live stop");
    require(asr.cancels == 1 && asr.starts == 2 && asr.finals == 1,
            "old session escaped live cancel");
}
}

int main() {
    try { segmentation(); preroll_max_cancel(); stress(); bounded_live(); normal_live_shutdown();
          live_cancel_keeps_capture(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "VAD_PIPELINE_TEST_PASS\n";
}
