#include "cockpit/voice/live_asr_pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;
using clock_type = std::chrono::steady_clock;
}

LiveAsrPipeline::LiveAsrPipeline(audio::IAudioCapture& capture, IAsrBackend& backend,
                                 VoiceSessionController& controller, AsrCallback callback,
                                 std::size_t capacity_override)
    : capture_(capture), backend_(backend), controller_(controller),
      callback_(std::move(callback)), capacity_override_(capacity_override) {}
LiveAsrPipeline::~LiveAsrPipeline() { if (running_) cancel(); }

Status LiveAsrPipeline::start(protocol::RequestId request_id, const audio::AudioFormat& requested) {
    if (running_ || capture_thread_.joinable() || decode_thread_.joinable())
        return {StatusCode::INVALID_STATE, "live session already running"};
    if (!callback_ || request_id == 0) return {StatusCode::INVALID_ARGUMENT, "live callback/request"};
    auto status = capture_.start(requested);
    if (!status.ok()) return status;
    const auto negotiated = capture_.actual_format();
    if (!negotiated || negotiated->sample_rate != 16000 || negotiated->channels != 1 ||
        negotiated->sample_format != audio::SampleFormat::S16_LE || negotiated->frames_per_buffer == 0) {
        capture_.stop();
        return {StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: negotiated capture"};
    }
    actual_ = *negotiated;
    token_ = controller_.start(request_id);
    if (token_.session_id == 0) { capture_.stop(); return {StatusCode::INVALID_STATE, "session start"}; }
    status = controller_.transition(token_, VoiceSessionState::Recognizing);
    if (!status.ok()) { capture_.stop(); return status; }
    status = backend_.start_session(token_, actual_, [this](const AsrEvent& event) { backend_event(event); });
    if (!status.ok()) {
        controller_.transition(token_, VoiceSessionState::Failed);
        capture_.stop();
        return status;
    }
    const std::size_t capacity = capacity_override_ ? capacity_override_ :
        std::clamp<std::size_t>((static_cast<std::size_t>(actual_.sample_rate) * 2 +
                actual_.frames_per_buffer - 1) / actual_.frames_per_buffer, 1, 1000);
    queue_ = std::make_unique<ipc::BoundedQueue<audio::PcmChunk>>(capacity);
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        metrics_ = {};
        metrics_.queue_capacity = capacity;
        failure_ = Status::Ok();
        started_at_ = clock_type::now();
        capture_stopped_at_ = {};
    }
    stopping_ = false;
    failed_ = false;
    final_seen_ = false;
    running_ = true;
    try {
        decode_thread_ = std::thread([this] { decode_loop(); });
        capture_thread_ = std::thread([this] { capture_loop(); });
    } catch (...) {
        stopping_ = true;
        controller_.cancel(token_);
        capture_.stop();
        queue_->close();
        if (capture_thread_.joinable()) capture_thread_.join();
        if (decode_thread_.joinable()) decode_thread_.join();
        backend_.cancel(token_);
        controller_.complete_cancel(token_);
        running_ = false;
        return {StatusCode::INTERNAL_ERROR, "live worker creation failed"};
    }
    return Status::Ok();
}

void LiveAsrPipeline::backend_event(const AsrEvent& event) {
    protocol::MessageType type = protocol::MessageType::ASR_ERROR;
    if (event.type == AsrEventType::PARTIAL) type = protocol::MessageType::ASR_PARTIAL;
    if (event.type == AsrEventType::FINAL) type = protocol::MessageType::ASR_FINAL;
    controller_.deliver_event(event.token, type, [&] {
        const auto now = clock_type::now();
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            if (event.type == AsrEventType::PARTIAL) {
                ++metrics_.partial_count;
                if (metrics_.first_partial_ms < 0)
                    metrics_.first_partial_ms = std::chrono::duration<double, std::milli>(now - started_at_).count();
            }
            if (event.type == AsrEventType::FINAL) {
                final_seen_ = true;
                if (capture_stopped_at_ != clock_type::time_point{})
                    metrics_.final_after_capture_stop_ms =
                        std::chrono::duration<double, std::milli>(now - capture_stopped_at_).count();
            }
        }
        callback_(event);
    });
}

void LiveAsrPipeline::fail(Status status) {
    bool expected = false;
    if (!failed_.compare_exchange_strong(expected, true)) return;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        failure_ = status;
    }
    backend_event({AsrEventType::ERROR, token_, 0, {}, status});
    controller_.cancel(token_);
    stopping_ = true;
    queue_->close();
}

void LiveAsrPipeline::capture_loop() {
    std::uint64_t sequence = 0;
    while (!stopping_) {
        auto result = capture_.read(std::chrono::milliseconds(100));
        if (stopping_) break;
        if (result.status.code == StatusCode::TIMEOUT) continue;
        if (!result.status.ok()) { fail(result.status); break; }
        result.buffer.session_id = token_.session_id;
        const auto frames = result.buffer.bytes.size() / 2U;
        if (frames == 0 || result.buffer.format.sample_rate != actual_.sample_rate ||
            result.buffer.format.channels != actual_.channels ||
            result.buffer.format.sample_format != actual_.sample_format) {
            fail({StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: capture chunk"});
            break;
        }
        audio::PcmChunk chunk{std::move(result.buffer), ++sequence, clock_type::now(), frames};
        const auto pushed = queue_->try_push(std::move(chunk));
        if (pushed == ipc::QueueStatus::FULL) {
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                ++metrics_.queue_overflow_count;
            }
            fail({StatusCode::UNAVAILABLE, "AUDIO_QUEUE_OVERFLOW"});
            break;
        }
        if (pushed == ipc::QueueStatus::CLOSED) break;
        std::lock_guard<std::mutex> lock(data_mutex_);
        metrics_.captured_frames += frames;
        metrics_.max_queue_depth = std::max(metrics_.max_queue_depth, queue_->size());
    }
}

void LiveAsrPipeline::decode_loop() {
    audio::PcmChunk chunk;
    while (queue_->pop_for(chunk, std::chrono::milliseconds(100)) != ipc::QueueStatus::CLOSED) {
        if (controller_.state() == VoiceSessionState::Cancelling) break;
        if (stopping_ && failed_) break;
        if (chunk.pcm.bytes.empty()) continue;
        if (chunk.pcm.session_id != token_.session_id) continue;
        auto status = backend_.push_audio(chunk.pcm);
        if (!status.ok()) { fail(status); break; }
        chunk = {};
    }
    if (!failed_ && controller_.state() == VoiceSessionState::Recognizing) {
        auto status = backend_.finish_input(token_);
        if (!status.ok()) fail(status);
        else if (final_seen_) {
            controller_.transition(token_, VoiceSessionState::Understanding);
            controller_.transition(token_, VoiceSessionState::Completed);
        }
    }
}

void LiveAsrPipeline::stop_workers(bool cancelled) {
    const auto stop_requested_at = clock_type::now();
    stopping_ = true;
    if (cancelled) controller_.cancel(token_); // fence old callbacks before joining
    capture_.stop();
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        capture_stopped_at_ = clock_type::now();
        metrics_.capture_stop_ms = std::chrono::duration<double, std::milli>(
            capture_stopped_at_ - stop_requested_at).count();
    }
    if (capture_thread_.joinable()) capture_thread_.join();
    queue_->close();
    if (decode_thread_.joinable()) decode_thread_.join();
    if (cancelled || failed_) {
        backend_.cancel(token_);
        controller_.complete_cancel(token_);
    }
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        metrics_.session_stop_ms = std::chrono::duration<double, std::milli>(
            clock_type::now() - stop_requested_at).count();
    }
    running_ = false;
}

Status LiveAsrPipeline::finish() {
    if (!running_) return {StatusCode::INVALID_STATE, "live session inactive"};
    stop_workers(false);
    if (failed_) { std::lock_guard<std::mutex> lock(data_mutex_); return failure_; }
    if (!final_seen_) return {StatusCode::INTERNAL_ERROR, "ASR_FINAL absent"};
    return Status::Ok();
}
Status LiveAsrPipeline::cancel() {
    if (!running_) return {StatusCode::INVALID_STATE, "live session inactive"};
    stop_workers(true);
    return Status::Ok();
}
LiveAsrMetrics LiveAsrPipeline::metrics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return metrics_;
}

}  // namespace cockpit::voice
