#include "cockpit/voice/vad_live_pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;
}

VadLivePipeline::VadLivePipeline(audio::IAudioCapture& capture, IVadBackend& vad, IAsrBackend& asr,
    VoiceSessionController& controller, AsrCallback asr_callback, VadCallback vad_callback,
    std::size_t capacity_override)
    : capture_(capture), processor_(vad, asr, controller, std::move(asr_callback),
      std::move(vad_callback)), capacity_override_(capacity_override) {}
VadLivePipeline::~VadLivePipeline() { if (running_) stop(); }

Status VadLivePipeline::start(const VadConfig& config, const audio::AudioFormat& requested) {
    if (running_ || capture_thread_.joinable() || processing_thread_.joinable())
        return {StatusCode::INVALID_STATE, "VAD live pipeline already running"};
    auto status = processor_.configure(config);
    if (!status.ok()) return status;
    status = capture_.start(requested);
    if (!status.ok()) return status;
    const auto actual = capture_.actual_format();
    if (!actual || actual->sample_rate != config.sample_rate || actual->channels != 1 ||
        actual->sample_format != audio::SampleFormat::S16_LE || actual->frames_per_buffer == 0) {
        capture_.stop();
        return {StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: negotiated VAD capture"};
    }
    actual_ = *actual;
    const std::size_t capacity = capacity_override_ ? capacity_override_ :
        std::clamp<std::size_t>((static_cast<std::size_t>(actual_.sample_rate) * 2 +
            actual_.frames_per_buffer - 1) / actual_.frames_per_buffer, 1, 1000);
    queue_ = std::make_unique<ipc::BoundedQueue<audio::PcmChunk>>(capacity);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_ = {};
        metrics_.queue_capacity = capacity;
        failure_ = Status::Ok();
    }
    failed_ = false;
    stopping_ = false;
    running_ = true;
    try {
        processing_thread_ = std::thread([this] { processing_loop(); });
        capture_thread_ = std::thread([this] { capture_loop(); });
    } catch (...) {
        stopping_ = true;
        capture_.stop();
        queue_->close();
        if (capture_thread_.joinable()) capture_thread_.join();
        if (processing_thread_.joinable()) processing_thread_.join();
        processor_.stop();
        running_ = false;
        return {StatusCode::INTERNAL_ERROR, "VAD worker creation failed"};
    }
    return Status::Ok();
}

void VadLivePipeline::fail(Status status) {
    bool expected = false;
    if (!failed_.compare_exchange_strong(expected, true)) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failure_ = std::move(status);
    }
    stopping_ = true;
    queue_->close();
}

void VadLivePipeline::capture_loop() {
    std::uint64_t sequence = 0;
    while (!stopping_) {
        auto result = capture_.read(std::chrono::milliseconds(100));
        if (stopping_) break;
        if (result.status.code == StatusCode::TIMEOUT) continue;
        if (!result.status.ok()) { fail(result.status); break; }
        const auto frames = result.buffer.bytes.size() / 2;
        if (frames == 0 || result.buffer.format.sample_rate != actual_.sample_rate ||
            result.buffer.format.channels != actual_.channels ||
            result.buffer.format.sample_format != actual_.sample_format ||
            (result.buffer.bytes.size() & 1U)) {
            fail({StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: VAD chunk"});
            break;
        }
        audio::PcmChunk chunk{std::move(result.buffer), ++sequence,
                              std::chrono::steady_clock::now(), frames};
        const auto pushed = queue_->try_push(std::move(chunk));
        if (pushed == ipc::QueueStatus::FULL) {
            { std::lock_guard<std::mutex> lock(mutex_); ++metrics_.queue_overflow_count; }
            fail({StatusCode::UNAVAILABLE, "AUDIO_QUEUE_OVERFLOW"});
            break;
        }
        if (pushed == ipc::QueueStatus::CLOSED) break;
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.captured_frames += frames;
        metrics_.queue_peak_depth = std::max(metrics_.queue_peak_depth, queue_->size());
    }
}

void VadLivePipeline::processing_loop() {
    audio::PcmChunk chunk;
    while (true) {
        const auto popped = queue_->pop_for(chunk, std::chrono::milliseconds(100));
        if (popped == ipc::QueueStatus::CLOSED) break;
        if (popped == ipc::QueueStatus::TIMEOUT) continue;
        if (failed_ || stopping_) break;
        auto status = processor_.accept(chunk);
        if (!status.ok()) { fail(status); break; }
        chunk = {};
    }
    if (!failed_ && !stopping_) {
        auto status = processor_.flush();
        if (!status.ok()) fail(status);
    }
    processor_.stop();
}

Status VadLivePipeline::cancel_current() { return processor_.cancel_current(); }

Status VadLivePipeline::stop() {
    if (!running_) return {StatusCode::INVALID_STATE, "VAD live pipeline inactive"};
    stopping_ = true;
    processor_.cancel_current(); // synchronously fence old ASR events before joining
    capture_.stop();
    if (capture_thread_.joinable()) capture_thread_.join();
    queue_->close();
    if (processing_thread_.joinable()) processing_thread_.join();
    running_ = false;
    if (failed_) { std::lock_guard<std::mutex> lock(mutex_); return failure_; }
    return Status::Ok();
}

VadLiveMetrics VadLivePipeline::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto value = metrics_;
    value.utterance = processor_.metrics();
    return value;
}

}  // namespace cockpit::voice
