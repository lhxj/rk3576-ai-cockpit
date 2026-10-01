#include "cockpit/voice/live_asr_pipeline.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace cockpit;
using namespace std::chrono_literals;

namespace {
class ScriptedCapture final : public audio::IAudioCapture {
public:
    explicit ScriptedCapture(int count, bool bad_format = false, bool read_error = false)
        : count_(count), bad_format_(bad_format), read_error_(read_error) {}
    protocol::Status start(const audio::AudioFormat& format) override {
        std::lock_guard<std::mutex> lock(mutex_);
        format_ = format;
        if (bad_format_) format_.sample_rate = 8000;
        delivered_ = 0;
        state_ = audio::AudioDeviceState::RUNNING;
        return protocol::Status::Ok();
    }
    audio::AudioCaptureResult read(std::chrono::milliseconds) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != audio::AudioDeviceState::RUNNING)
            return {{protocol::StatusCode::CANCELLED, "stopped"}, {}};
        if (read_error_ && delivered_ >= 1)
            return {{protocol::StatusCode::INTERNAL_ERROR, "fixture capture failed"}, {}};
        if (delivered_++ >= count_) {
            std::this_thread::sleep_for(1ms);
            return {{protocol::StatusCode::TIMEOUT, "done"}, {}};
        }
        audio::PcmBuffer pcm;
        pcm.format = format_;
        pcm.bytes.resize(640);
        return {protocol::Status::Ok(), std::move(pcm)};
    }
    void stop() override { std::lock_guard<std::mutex> lock(mutex_); state_ = audio::AudioDeviceState::STOPPED; }
    audio::AudioDeviceState state() const override {
        std::lock_guard<std::mutex> lock(mutex_); return state_;
    }
    std::optional<audio::AudioFormat> actual_format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != audio::AudioDeviceState::RUNNING) return std::nullopt;
        return format_;
    }
private:
    int count_;
    bool bad_format_;
    bool read_error_;
    mutable std::mutex mutex_;
    int delivered_{0};
    audio::AudioFormat format_;
    audio::AudioDeviceState state_{audio::AudioDeviceState::STOPPED};
};

class FakeAsr final : public voice::IAsrBackend {
public:
    explicit FakeAsr(bool slow = false, bool fail = false) : slow_(slow), fail_(fail) {}
    protocol::Status start_session(voice::SessionToken token, const audio::AudioFormat&, voice::AsrCallback callback) override {
        token_ = token; callback_ = std::move(callback); active_ = true; return protocol::Status::Ok();
    }
    protocol::Status push_audio(const audio::PcmBuffer&) override {
        if (!active_) return {protocol::StatusCode::CANCELLED, "inactive"};
        if (slow_) std::this_thread::sleep_for(10ms);
        if (fail_) return {protocol::StatusCode::INTERNAL_ERROR, "fixture backend failed"};
        callback_({voice::AsrEventType::PARTIAL, token_, 1, "partial", protocol::Status::Ok()});
        return protocol::Status::Ok();
    }
    protocol::Status finish_input(voice::SessionToken) override {
        if (!active_) return {protocol::StatusCode::CANCELLED, "inactive"};
        callback_({voice::AsrEventType::FINAL, token_, 2, "final", protocol::Status::Ok()});
        active_ = false;
        return protocol::Status::Ok();
    }
    void cancel(voice::SessionToken) override { active_ = false; }
private:
    bool slow_;
    bool fail_;
    bool active_{false};
    voice::SessionToken token_;
    voice::AsrCallback callback_;
};
}

int main() {
    {
        ScriptedCapture capture(8);
        FakeAsr asr;
        voice::VoiceSessionController controller(1);
        std::mutex events_mutex;
        std::vector<voice::AsrEvent> events;
        voice::LiveAsrPipeline pipeline(capture, asr, controller, [&](const voice::AsrEvent& e) {
            std::lock_guard<std::mutex> lock(events_mutex); events.push_back(e);
        });
        assert(pipeline.start(1).ok());
        std::this_thread::sleep_for(30ms);
        assert(pipeline.finish().ok());
        assert(capture.state() == audio::AudioDeviceState::STOPPED);
        assert(controller.state() == voice::VoiceSessionState::Completed);
        assert(!events.empty() && events.back().type == voice::AsrEventType::FINAL);
        assert(pipeline.metrics().captured_frames > 0);
        assert(pipeline.start(2).ok());
        std::this_thread::sleep_for(20ms);
        assert(pipeline.cancel().ok());
        const auto old = pipeline.token().session_id;
        for (const auto& event : events)
            assert(event.token.session_id != old || event.type != voice::AsrEventType::FINAL);
        assert(pipeline.start(3).ok());
        std::this_thread::sleep_for(20ms);
        assert(pipeline.finish().ok());
        assert(events.back().token.session_id != old);
    }
    {
        ScriptedCapture capture(1000);
        FakeAsr asr(true);
        voice::VoiceSessionController controller(2);
        int errors = 0;
        voice::LiveAsrPipeline pipeline(capture, asr, controller, [&](const voice::AsrEvent& e) {
            if (e.type == voice::AsrEventType::ERROR) ++errors;
        }, 1);
        assert(pipeline.start(1).ok());
        std::this_thread::sleep_for(30ms);
        const auto result = pipeline.finish();
        assert(!result.ok() && result.detail == "AUDIO_QUEUE_OVERFLOW");
        assert(pipeline.metrics().queue_overflow_count == 1 && errors == 1);
    }
    {
        ScriptedCapture capture(1, true);
        FakeAsr asr;
        voice::VoiceSessionController controller(3);
        voice::LiveAsrPipeline pipeline(capture, asr, controller, [](const voice::AsrEvent&) {});
        assert(!pipeline.start(1).ok());
        assert(capture.state() == audio::AudioDeviceState::STOPPED);
    }
    for (bool capture_error : {false, true}) {
        ScriptedCapture capture(4, false, capture_error);
        FakeAsr asr(false, !capture_error);
        voice::VoiceSessionController controller(4);
        int errors = 0;
        voice::LiveAsrPipeline pipeline(capture, asr, controller, [&](const voice::AsrEvent& e) {
            if (e.type == voice::AsrEventType::ERROR) ++errors;
        });
        assert(pipeline.start(1).ok());
        std::this_thread::sleep_for(20ms);
        assert(!pipeline.finish().ok());
        assert(errors == 1 && capture.state() == audio::AudioDeviceState::STOPPED);
    }
    return 0;
}
