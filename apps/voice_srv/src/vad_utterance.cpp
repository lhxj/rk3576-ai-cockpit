#include "cockpit/voice/vad_utterance.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;
using clock_type = std::chrono::steady_clock;
}

VadUtteranceProcessor::VadUtteranceProcessor(IVadBackend& vad, IAsrBackend& asr,
        VoiceSessionController& controller, AsrCallback asr_callback, VadCallback vad_callback)
    : vad_(vad), asr_(asr), controller_(controller), asr_callback_(std::move(asr_callback)),
      vad_callback_(std::move(vad_callback)) {}

Status VadUtteranceProcessor::configure(const VadConfig& config) {
    if (configured_) return {StatusCode::INVALID_STATE, "utterance processor already configured"};
    if (!asr_callback_) return {StatusCode::INVALID_ARGUMENT, "ASR callback required"};
    auto status = validate_vad_config(config);
    if (!status.ok()) return status;
    status = vad_.configure(config);
    if (!status.ok()) return status;
    config_ = config;
    configured_ = true;
    return Status::Ok();
}

VadUtteranceMetrics VadUtteranceProcessor::metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}
std::size_t VadUtteranceProcessor::pre_roll_size_frames() const { return pre_roll_.size() / 2; }

void VadUtteranceProcessor::append_pre_roll(const audio::PcmBuffer& pcm) {
    const auto max_bytes = static_cast<std::size_t>(config_.sample_rate) * config_.pre_roll_ms / 1000 * 2;
    for (auto byte : pcm.bytes) {
        if (pre_roll_.size() == max_bytes) pre_roll_.pop_front();
        pre_roll_.push_back(byte);
    }
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.peak_pre_roll_frames = std::max(metrics_.peak_pre_roll_frames, pre_roll_.size() / 2);
}

void VadUtteranceProcessor::backend_event(const AsrEvent& event) {
    auto type = protocol::MessageType::ASR_ERROR;
    if (event.type == AsrEventType::PARTIAL) type = protocol::MessageType::ASR_PARTIAL;
    if (event.type == AsrEventType::FINAL) type = protocol::MessageType::ASR_FINAL;
    controller_.deliver_event(event.token, type, [&] {
        const auto now = clock_type::now();
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            if (event.type == AsrEventType::PARTIAL) {
                ++metrics_.partial_count;
                if (metrics_.speech_start_to_first_partial_ms < 0)
                    metrics_.speech_start_to_first_partial_ms =
                        std::chrono::duration<double, std::milli>(now - speech_started_at_).count();
            }
            if (event.type == AsrEventType::FINAL) {
                final_seen_ = true;
                metrics_.speech_end_to_final_ms =
                    std::chrono::duration<double, std::milli>(now - speech_ended_at_).count();
            }
        }
        asr_callback_(event);
    });
}

Status VadUtteranceProcessor::start_utterance(clock_type::time_point at) {
    state_ = UtteranceState::Starting;
    const auto token = controller_.start(next_request_id_++);
    if (!token.session_id) return {StatusCode::INVALID_STATE, "ASR session allocation"};
    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        token_ = token;
    }
    auto status = controller_.transition(token, VoiceSessionState::Recognizing);
    if (!status.ok()) return status;
    audio::AudioFormat format{};
    status = asr_.start_session(token, format, [this](const AsrEvent& event) { backend_event(event); });
    if (!status.ok()) { controller_.transition(token, VoiceSessionState::Failed); return status; }
    speech_started_at_ = at;
    final_seen_ = false;
    active_frames_ = 0;
    state_ = UtteranceState::Speaking;
    if (!pre_roll_.empty()) {
        audio::PcmBuffer pcm{format, token.session_id, {pre_roll_.begin(), pre_roll_.end()}};
        const auto frames = pcm.bytes.size() / 2;
        status = feed(pcm);
        if (!status.ok()) return status;
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.pre_roll_frames += frames;
    }
    pre_roll_.clear();
    return Status::Ok();
}

Status VadUtteranceProcessor::feed(const audio::PcmBuffer& source) {
    SessionToken token;
    { std::lock_guard<std::mutex> lock(token_mutex_); token = token_; }
    auto pcm = source;
    pcm.session_id = token.session_id;
    auto status = asr_.push_audio(pcm);
    if (status.ok()) {
        active_frames_ += pcm.bytes.size() / 2;
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.speech_frames += pcm.bytes.size() / 2;
    }
    return status;
}

Status VadUtteranceProcessor::finish_utterance(bool forced, clock_type::time_point at) {
    if (state_ != UtteranceState::Speaking) return Status::Ok();
    state_ = UtteranceState::Finalizing;
    speech_ended_at_ = at;
    SessionToken token;
    { std::lock_guard<std::mutex> lock(token_mutex_); token = token_; }
    auto status = asr_.finish_input(token);
    if (!status.ok() || !final_seen_) {
        controller_.transition(token, VoiceSessionState::Failed);
        state_ = UtteranceState::Error;
        return status.ok() ? Status{StatusCode::INTERNAL_ERROR, "ASR_FINAL absent"} : status;
    }
    controller_.transition(token, VoiceSessionState::Understanding);
    controller_.transition(token, VoiceSessionState::Completed);
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.utterance_count;
        if (forced) ++metrics_.forced_max_duration_count;
    }
    { std::lock_guard<std::mutex> lock(token_mutex_); token_ = {}; }
    active_frames_ = 0;
    pre_roll_.clear();
    state_ = UtteranceState::Listening;
    return Status::Ok();
}

void VadUtteranceProcessor::cancel_on_worker() {
    if (state_ != UtteranceState::Speaking && state_ != UtteranceState::Starting) return;
    SessionToken token;
    { std::lock_guard<std::mutex> lock(token_mutex_); token = token_; }
    if (controller_.state() != VoiceSessionState::Cancelling) controller_.cancel(token);
    asr_.cancel(token);
    controller_.complete_cancel(token);
    { std::lock_guard<std::mutex> lock(token_mutex_); token_ = {}; }
    active_frames_ = 0;
    pre_roll_.clear();
    state_ = UtteranceState::Cancelled;
}

Status VadUtteranceProcessor::cancel_current() {
    if (state_ != UtteranceState::Speaking && state_ != UtteranceState::Starting)
        return {StatusCode::INVALID_STATE, "no active utterance"};
    cancel_requested_ = true;
    SessionToken token;
    { std::lock_guard<std::mutex> lock(token_mutex_); token = token_; }
    if (token.session_id) controller_.cancel(token); // fence callbacks synchronously
    return Status::Ok();
}

void VadUtteranceProcessor::fail(Status status) {
    SessionToken token;
    { std::lock_guard<std::mutex> lock(token_mutex_); token = token_; token_ = {}; }
    if (token.session_id) {
        backend_event({AsrEventType::ERROR, token, 0, {}, status});
        controller_.cancel(token);
        asr_.cancel(token);
        controller_.complete_cancel(token);
    }
    state_ = UtteranceState::Error;
}

Status VadUtteranceProcessor::process_events(const std::vector<VadEvent>& events,
                                              clock_type::time_point at) {
    for (const auto& event : events) {
        if (vad_callback_) vad_callback_(event);
        if (event.type == VadEventType::SpeechStarted) {
            { std::lock_guard<std::mutex> lock(metrics_mutex_); ++metrics_.vad_speech_start_count; }
            if (state_ == UtteranceState::Listening) {
                auto status = start_utterance(at);
                if (!status.ok()) return status;
            }
        } else if (event.type == VadEventType::SpeechEnded) {
            { std::lock_guard<std::mutex> lock(metrics_mutex_); ++metrics_.vad_speech_end_count; }
            if (state_ == UtteranceState::Cancelled) {
                state_ = UtteranceState::Listening;
                pre_roll_.clear();
            } else if (state_ == UtteranceState::Speaking) {
                auto status = finish_utterance(false, at);
                if (!status.ok()) return status;
            } else {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                ++metrics_.rejected_short_utterance_count;
            }
        } else return {StatusCode::INTERNAL_ERROR, "VAD backend error"};
    }
    return Status::Ok();
}

Status VadUtteranceProcessor::accept(const audio::PcmChunk& chunk) {
    if (!configured_ || state_ == UtteranceState::Error)
        return {StatusCode::INVALID_STATE, "VAD processor inactive"};
    if (chunk.frames == 0 || chunk.frames > 3200 || chunk.pcm.bytes.size() != chunk.frames * 2 ||
        chunk.pcm.format.sample_rate != config_.sample_rate || chunk.pcm.format.channels != 1 ||
        chunk.pcm.format.sample_format != audio::SampleFormat::S16_LE)
        return {StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: PCM chunk"};
    if (expected_sequence_ && chunk.sequence != expected_sequence_) {
        { std::lock_guard<std::mutex> lock(metrics_mutex_); ++metrics_.pcm_sequence_gap_count; }
        fail({StatusCode::UNAVAILABLE, "PCM_SEQUENCE_GAP"});
        return {StatusCode::UNAVAILABLE, "PCM_SEQUENCE_GAP"};
    }
    expected_sequence_ = chunk.sequence + 1;
    if (cancel_requested_.exchange(false)) cancel_on_worker();
    const bool was_speaking = state_ == UtteranceState::Speaking;
    if (state_ == UtteranceState::Listening) {
        append_pre_roll(chunk.pcm);
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.silence_frames += chunk.frames;
    }
    const auto begin = clock_type::now();
    auto detected = vad_.accept_samples(chunk.pcm, chunk.captured_at);
    { std::lock_guard<std::mutex> lock(metrics_mutex_);
      metrics_.vad_processing_ms += std::chrono::duration<double, std::milli>(clock_type::now() - begin).count(); }
    if (!detected.status.ok()) { fail(detected.status); return detected.status; }
    if (was_speaking && state_ == UtteranceState::Speaking) {
        auto status = feed(chunk.pcm);
        if (!status.ok()) { fail(status); return status; }
    }
    auto status = process_events(detected.events, chunk.captured_at);
    if (!status.ok()) { fail(status); return status; }
    if (state_ == UtteranceState::Speaking &&
        active_frames_ >= static_cast<std::uint64_t>(config_.sample_rate) * config_.max_utterance_ms / 1000) {
        status = finish_utterance(true, chunk.captured_at);
        vad_.reset();
    }
    return status;
}

Status VadUtteranceProcessor::flush() {
    if (!configured_) return {StatusCode::INVALID_STATE, "VAD processor inactive"};
    if (cancel_requested_.exchange(false)) cancel_on_worker();
    auto result = vad_.flush();
    if (!result.status.ok()) return result.status;
    auto status = process_events(result.events, clock_type::now());
    if (!status.ok()) return status;
    if (state_ == UtteranceState::Speaking) return finish_utterance(false, clock_type::now());
    if (state_ == UtteranceState::Cancelled) state_ = UtteranceState::Listening;
    return Status::Ok();
}

void VadUtteranceProcessor::stop() {
    if (cancel_requested_.exchange(false) || state_ == UtteranceState::Speaking ||
        state_ == UtteranceState::Starting) cancel_on_worker();
    vad_.reset();
    pre_roll_.clear();
    state_ = UtteranceState::Listening;
}

}  // namespace cockpit::voice
