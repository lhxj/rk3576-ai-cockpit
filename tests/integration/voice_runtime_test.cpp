#include "../unit/vehicle_core/test_support.hpp"

#include "cockpit/vehicle/voice_command_sink.hpp"
#include "cockpit/voice/voice_runtime.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace {
using namespace cockpit;
using namespace cockpit::vehicle;
using namespace cockpit::vehicle::test;

void require(bool value, const char* detail) {
    if (!value) throw std::runtime_error(detail);
}

class RestartableCapture final : public audio::IAudioCapture {
public:
    protocol::Status start(const audio::AudioFormat& format) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return {protocol::StatusCode::INVALID_STATE, "capture active"};
        running_ = true;
        format_ = format;
        chunks_.clear();
        ++starts;
        return protocol::Status::Ok();
    }
    audio::AudioCaptureResult read(std::chrono::milliseconds timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!ready_.wait_for(lock, timeout, [&] { return !running_ || !chunks_.empty(); }))
            return {{protocol::StatusCode::TIMEOUT, "fixture wait"}, {}};
        if (!running_) return {{protocol::StatusCode::CANCELLED, "fixture stopped"}, {}};
        auto value = std::move(chunks_.front());
        chunks_.pop_front();
        return {protocol::Status::Ok(), std::move(value)};
    }
    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) ++stops;
        running_ = false;
        ready_.notify_all();
    }
    audio::AudioDeviceState state() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_ ? audio::AudioDeviceState::RUNNING : audio::AudioDeviceState::STOPPED;
    }
    std::optional<audio::AudioFormat> actual_format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_ ? std::optional<audio::AudioFormat>(format_) : std::nullopt;
    }
    void push(std::int16_t sample, std::size_t frames = 320) {
        audio::PcmBuffer buffer;
        buffer.format = format_;
        buffer.bytes.reserve(frames * 2);
        for (std::size_t i = 0; i < frames; ++i) {
            const auto raw = static_cast<std::uint16_t>(sample);
            buffer.bytes.push_back(static_cast<std::uint8_t>(raw));
            buffer.bytes.push_back(static_cast<std::uint8_t>(raw >> 8));
        }
        std::lock_guard<std::mutex> lock(mutex_);
        require(running_, "push to stopped capture");
        chunks_.push_back(std::move(buffer));
        ready_.notify_one();
    }
    int starts{0};
    int stops{0};
private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<audio::PcmBuffer> chunks_;
    audio::AudioFormat format_;
    bool running_{false};
};

class StartOnSignalVad final : public voice::IVadBackend {
public:
    protocol::Status configure(const voice::VadConfig& config) override {
        config_ = config;
        ++loads;
        reset();
        return protocol::Status::Ok();
    }
    void reset() override { speaking_ = false; }
    voice::VadAcceptResult accept_samples(const audio::PcmBuffer& pcm,
            std::chrono::steady_clock::time_point at) override {
        voice::VadAcceptResult result{protocol::Status::Ok(), {}};
        const auto raw = static_cast<std::uint16_t>(pcm.bytes[0] | (pcm.bytes[1] << 8));
        if (raw != 0 && !speaking_) {
            speaking_ = true;
            result.events.push_back({voice::VadEventType::SpeechStarted, 1, at, -1.0F});
        }
        return result;
    }
    voice::VadAcceptResult flush() override { return {protocol::Status::Ok(), {}}; }
    voice::VadState state() const override {
        return speaking_ ? voice::VadState::SpeechActive : voice::VadState::Silence;
    }
    int loads{0};
private:
    voice::VadConfig config_;
    bool speaking_{false};
};

class ControlledAsr final : public voice::IAsrBackend {
public:
    protocol::Status start_session(voice::SessionToken token, const audio::AudioFormat&,
                                   voice::AsrCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        token_ = token;
        callback_ = std::move(callback);
        active_ = true;
        ready_.notify_all();
        return protocol::Status::Ok();
    }
    protocol::Status push_audio(const audio::PcmBuffer&) override {
        return protocol::Status::Ok();
    }
    protocol::Status finish_input(voice::SessionToken) override {
        return protocol::Status::Ok();
    }
    void cancel(voice::SessionToken) override {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ = false;
    }
    bool wait_active() {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, 1s, [&] { return active_; });
    }
    void emit_late_final(const std::string& text) {
        voice::AsrCallback callback;
        voice::SessionToken token;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = callback_;
            token = token_;
        }
        if (callback) callback({voice::AsrEventType::FINAL, token, 99, text,
                                protocol::Status::Ok()});
    }
private:
    std::mutex mutex_;
    std::condition_variable ready_;
    voice::AsrCallback callback_;
    voice::SessionToken token_;
    bool active_{false};
};

class Reports {
public:
    void add(const voice::IntentDispatchReport& report) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(report);
        ready_.notify_all();
    }
    bool wait(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, 2s, [&] { return values_.size() >= count; });
    }
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.size();
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<voice::IntentDispatchReport> values_;
};

struct Harness {
    Harness()
        : fixture(), sink(fixture.client, fixture.clock, 77), runtime(capture, vad, asr,
          controller, router, sink, [this] { return fixture.clock->now_ms(); }, {},
          [this](const voice::IntentDispatchReport& report) { reports.add(report); }) {}

    void start() { require(fixture.start_status.ok() && runtime.start().ok(), "runtime start"); }
    void final(const std::string& text, std::size_t report_count, bool duplicate = false) {
        require(runtime.inject_final_for_test(text, duplicate).ok(), "synthetic FINAL enqueue");
        require(reports.wait(report_count), "intent report timeout");
    }

    Fixture fixture;
    RestartableCapture capture;
    StartOnSignalVad vad;
    ControlledAsr asr;
    voice::VoiceSessionController controller{77};
    voice::DeterministicIntentRouter router;
    VehicleCommandSinkAdapter sink;
    Reports reports;
    voice::VoiceRuntime runtime;
};

void complete_flow() {
    Harness h;
    h.start();
    h.final("打开摄像头", 1);
    require(wait_until([&] {
                return h.fixture.media->invocation_count(CommandType::CAMERA_PREVIEW_START) == 1;
            }),
            "open camera not executed");
    h.final("关闭摄像头", 2);
    require(wait_until([&] {
                return h.fixture.media->invocation_count(CommandType::CAMERA_PREVIEW_STOP) == 1;
            }),
            "close camera not executed");
    h.final("开始录像", 4, true);
    require(wait_until([&] {
                return h.fixture.media->invocation_count(CommandType::RECORDING_START) == 1;
            }),
            "duplicate FINAL executed twice");
    h.final("停止录像", 5);
    require(wait_until([&] {
                return h.fixture.media->invocation_count(CommandType::RECORDING_STOP) == 1;
            }),
            "stop recording not executed");
    const auto command_count = [&] {
        return h.fixture.media->invocation_count(CommandType::RECORDING_STOP) +
            h.fixture.media->invocation_count(CommandType::RECORDING_START) +
            h.fixture.media->invocation_count(CommandType::CAMERA_PREVIEW_START) +
            h.fixture.media->invocation_count(CommandType::CAMERA_PREVIEW_STOP);
    };
    require(h.runtime.inject_partial_for_test("打开摄像头").ok(), "PARTIAL injection");
    std::this_thread::sleep_for(20ms);
    require(h.reports.size() == 5, "PARTIAL entered dispatcher");
    h.final("今天天气怎么样", 6);
    h.final("不要打开摄像头", 7);
    require(command_count() == 4, "rejected intent executed");
    const auto metrics = h.runtime.metrics();
    require(metrics.intent_match_count == 4 && metrics.duplicate_final_count == 1,
            "runtime match/dedupe metrics");
    require(metrics.no_match_count == 1 && metrics.rejected_count >= 1,
            "runtime rejection metrics");
    require(h.runtime.stop().ok(), "runtime stop");
    require(h.capture.state() == audio::AudioDeviceState::STOPPED, "capture leaked");
}

void cancel_and_late_final() {
    Harness h;
    h.start();
    h.capture.push(4000);
    require(h.asr.wait_active(), "ASR session did not start");
    require(h.runtime.cancel_current().ok(), "runtime cancel");
    h.asr.emit_late_final("打开摄像头");
    std::this_thread::sleep_for(20ms);
    require(h.reports.size() == 0, "cancelled FINAL reached dispatcher");
    require(h.fixture.media->invocation_count(CommandType::CAMERA_PREVIEW_START) == 0,
            "cancelled FINAL reached Core");
    require(h.runtime.stop().ok(), "cancel runtime stop");
}

void stop_race_and_fifty_cycles() {
    Harness h;
    for (int cycle = 0; cycle < 50; ++cycle) {
        require(h.runtime.start().ok(), "50-cycle start");
        std::thread late([&] {
            while (h.runtime.state() == voice::VoiceRuntimeState::STARTING)
                std::this_thread::yield();
            (void)h.runtime.inject_final_for_test("无匹配命令");
        });
        require(h.runtime.stop().ok(), "50-cycle stop");
        late.join();
        require(h.runtime.state() == voice::VoiceRuntimeState::STOPPED,
                "runtime did not stop");
        require(h.capture.state() == audio::AudioDeviceState::STOPPED,
                "50-cycle capture leaked");
    }
    require(h.capture.starts == 50 && h.capture.stops == 50, "capture lifecycle count");
    require(h.vad.loads == 1, "VAD reloaded across start/stop");
    require(h.runtime.inject_final_for_test("打开摄像头").code ==
                protocol::StatusCode::INVALID_STATE,
            "FINAL accepted after stop");
}
}  // namespace

int main() {
    try {
        complete_flow();
        cancel_and_late_final();
        stop_race_and_fifty_cycles();
        std::cout << "VOICE_RUNTIME_ORCHESTRATION_HOST_PASS cycles=50\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "voice_runtime_test: " << error.what() << '\n';
        return 1;
    }
}
