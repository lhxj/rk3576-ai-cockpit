#include "cockpit/voice/voice_runtime.hpp"

#include <limits>
#include <utility>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;
}

VoiceRuntime::VoiceRuntime(audio::IAudioCapture& capture, IVadBackend& vad, IAsrBackend& asr,
    VoiceSessionController& controller, const DeterministicIntentRouter& router,
    IVoiceSessionCommandSink& command_sink, Clock clock, VoiceRuntimeConfig config,
    ReportCallback report, XrunCounter xrun_counter)
    : capture_(capture), controller_(controller), command_sink_(command_sink),
      clock_(std::move(clock)), config_(std::move(config)), report_(std::move(report)),
      xrun_counter_(std::move(xrun_counter)),
      dispatcher_(router, controller, command_sink, clock_,
          [this](const IntentDispatchReport& value) { on_dispatch_report(value); },
          config_.dispatch_queue_capacity, config_.recent_final_capacity,
          [this](SessionToken token, protocol::Deadline deadline) {
              if (!accepting_events_)
                  return Status{StatusCode::CANCELLED, "voice runtime stopping"};
              return command_sink_.activate_session(token, deadline);
          }),
      pipeline_(capture, vad, asr, controller,
          [this](const AsrEvent& event) { on_asr_event(event); },
          [this](const VadEvent& event) { on_vad_event(event); },
          config_.audio_queue_capacity) {}

VoiceRuntime::~VoiceRuntime() {
    if (state() != VoiceRuntimeState::STOPPED) (void)stop();
}

void VoiceRuntime::set_state(VoiceRuntimeState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

Status VoiceRuntime::start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != VoiceRuntimeState::STOPPED)
            return {StatusCode::INVALID_STATE, "voice runtime already active"};
        if (!clock_ || config_.intent_timeout.count() <= 0 ||
            config_.dispatch_queue_capacity == 0 || config_.recent_final_capacity == 0)
            return {StatusCode::INVALID_ARGUMENT, "voice runtime configuration"};
        state_ = VoiceRuntimeState::STARTING;
    }
    auto status = dispatcher_.start();
    if (!status.ok()) { set_state(VoiceRuntimeState::ERROR); return status; }
    accepting_events_ = true;
    status = pipeline_.start(config_.vad, config_.requested_audio);
    if (!status.ok()) {
        accepting_events_ = false;
        dispatcher_.stop();
        set_state(VoiceRuntimeState::ERROR);
        return status;
    }
    set_state(VoiceRuntimeState::LISTENING);
    return Status::Ok();
}

Status VoiceRuntime::cancel_current() {
    const auto token = controller_.current();
    auto pipeline_status = pipeline_.cancel_current();
    auto controller_status = controller_.cancel(token);
    auto sink_status = command_sink_.cancel_session(token);
    if (controller_status.ok()) (void)controller_.complete_cancel(token);
    if (pipeline_status.ok() || controller_status.ok() || sink_status.ok()) return Status::Ok();
    return controller_status;
}

Status VoiceRuntime::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == VoiceRuntimeState::STOPPED)
            return {StatusCode::INVALID_STATE, "voice runtime inactive"};
        if (state_ == VoiceRuntimeState::STOPPING)
            return {StatusCode::INVALID_STATE, "voice runtime already stopping"};
        state_ = VoiceRuntimeState::STOPPING;
    }
    accepting_events_ = false;

    // Close ALSA first. Controller cancellation then fences a FINAL already in
    // the dispatcher queue before the queue is closed and drained.
    const bool pipeline_active = pipeline_.running();
    if (pipeline_active) (void)pipeline_.begin_stop();
    const auto token = controller_.current();
    if (token.session_id != 0) {
        const auto cancelled = controller_.cancel(token);
        (void)command_sink_.cancel_session(token);
        if (cancelled.ok()) (void)controller_.complete_cancel(token);
    }
    auto pipeline_status = pipeline_active ? pipeline_.stop() : Status::Ok();
    dispatcher_.stop();
    set_state(VoiceRuntimeState::STOPPED);
    return pipeline_status;
}

void VoiceRuntime::on_asr_event(const AsrEvent& event) {
    if (event.type != AsrEventType::FINAL) {
        if (event.type == AsrEventType::ERROR) set_state(VoiceRuntimeState::ERROR);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++counters_.final_count;
        if (!accepting_events_) { ++counters_.rejected_count; return; }
    }
    const auto now = clock_ ? clock_() : 0;
    if (now == 0 || static_cast<std::uint64_t>(config_.intent_timeout.count()) >
            std::numeric_limits<protocol::Deadline>::max() - now) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++counters_.rejected_count;
        state_ = VoiceRuntimeState::ERROR;
        return;
    }
    const auto status = dispatcher_.enqueue_event(
        event, now + static_cast<protocol::Deadline>(config_.intent_timeout.count()));
    if (!status.ok()) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++counters_.rejected_count;
        state_ = VoiceRuntimeState::ERROR;
    }
}

void VoiceRuntime::on_vad_event(const VadEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == VoiceRuntimeState::STOPPING || state_ == VoiceRuntimeState::STOPPED) return;
    if (event.type == VadEventType::SpeechStarted && accepting_events_)
        state_ = VoiceRuntimeState::PROCESSING;
    if (event.type == VadEventType::Error) state_ = VoiceRuntimeState::ERROR;
}

void VoiceRuntime::on_dispatch_report(const IntentDispatchReport& value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (value.duplicate) {
            ++counters_.duplicate_final_count;
        } else if (value.outcome == IntentOutcome::MATCH) {
            ++counters_.intent_match_count;
            if (!value.submitted) ++counters_.rejected_count;
        } else if (value.outcome == IntentOutcome::NO_MATCH) {
            ++counters_.no_match_count;
        } else {
            ++counters_.rejected_count;
        }
    }

    if (!value.duplicate) {
        if (controller_.state() == VoiceSessionState::Recognizing)
            (void)controller_.transition(value.token, VoiceSessionState::Understanding);
        if (controller_.state() == VoiceSessionState::Understanding)
            (void)controller_.transition(value.token, VoiceSessionState::Completed);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (accepting_events_ && state_ != VoiceRuntimeState::STOPPING &&
            state_ != VoiceRuntimeState::STOPPED)
            state_ = VoiceRuntimeState::LISTENING;
    }
    if (report_) {
        try { report_(value); } catch (...) {}
    }
}

Status VoiceRuntime::inject_for_test(AsrEventType type, const std::string& text, bool duplicate) {
    if (!accepting_events_) return {StatusCode::INVALID_STATE, "voice runtime not accepting events"};
    const auto token = controller_.start(next_test_request_.fetch_add(1));
    if (!token.session_id) return {StatusCode::INVALID_STATE, "test session allocation"};
    auto status = controller_.transition(token, VoiceSessionState::Recognizing);
    if (!status.ok()) return status;
    const AsrEvent event{type, token, next_test_sequence_.fetch_add(1), text, Status::Ok()};
    const auto message = type == AsrEventType::FINAL ? protocol::MessageType::ASR_FINAL
                                                      : protocol::MessageType::ASR_PARTIAL;
    Status queued;
    status = controller_.deliver_event(token, message, [&] {
        on_asr_event(event);
        if (duplicate) on_asr_event(event);
        queued = Status::Ok();
    });
    if (!status.ok()) return status;
    if (type == AsrEventType::PARTIAL) {
        (void)controller_.cancel(token);
        (void)controller_.complete_cancel(token);
    }
    return queued;
}

Status VoiceRuntime::inject_final_for_test(const std::string& text, bool duplicate) {
    return inject_for_test(AsrEventType::FINAL, text, duplicate);
}

Status VoiceRuntime::inject_partial_for_test(const std::string& text) {
    return inject_for_test(AsrEventType::PARTIAL, text, false);
}

VoiceRuntimeState VoiceRuntime::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pipeline_.failed() && state_ != VoiceRuntimeState::STOPPING &&
        state_ != VoiceRuntimeState::STOPPED) return VoiceRuntimeState::ERROR;
    return state_;
}

VoiceRuntimeMetrics VoiceRuntime::metrics() const {
    VoiceRuntimeMetrics value;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        value = counters_;
    }
    const auto pipeline = pipeline_.metrics();
    const auto dispatch = dispatcher_.stats();
    value.utterance_count = pipeline.utterance.utterance_count;
    value.audio_queue_peak = pipeline.queue_peak_depth;
    value.audio_overflow_count = pipeline.queue_overflow_count;
    value.dispatch_queue_peak = dispatch.queue_peak;
    value.xrun_count = xrun_counter_ ? xrun_counter_() : 0;
    return value;
}

const char* voice_runtime_state_name(VoiceRuntimeState state) {
    switch (state) {
        case VoiceRuntimeState::STOPPED: return "STOPPED";
        case VoiceRuntimeState::STARTING: return "STARTING";
        case VoiceRuntimeState::LISTENING: return "LISTENING";
        case VoiceRuntimeState::PROCESSING: return "PROCESSING";
        case VoiceRuntimeState::STOPPING: return "STOPPING";
        case VoiceRuntimeState::ERROR: return "ERROR";
    }
    return "ERROR";
}

}  // namespace cockpit::voice
