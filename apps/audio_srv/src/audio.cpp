#include "cockpit/audio/audio.hpp"

#include <algorithm>
#include <utility>

namespace cockpit::audio {
namespace {
std::size_t bytes_per_sample(SampleFormat format) { return format == SampleFormat::S16_LE ? 2U : 4U; }
bool same_format(const AudioFormat& a, const AudioFormat& b) {
    return a.sample_rate == b.sample_rate && a.channels == b.channels &&
           a.sample_format == b.sample_format;
}
}  // namespace

protocol::Status validate_format(const AudioFormat& format) {
    if (format.sample_rate == 0 || format.channels == 0 || format.channels > 8 ||
        format.frames_per_buffer == 0 || format.frames_per_buffer > 65536 ||
        (format.sample_format != SampleFormat::S16_LE && format.sample_format != SampleFormat::F32_LE))
        return {protocol::StatusCode::INVALID_ARGUMENT, "audio format"};
    return protocol::Status::Ok();
}

protocol::Status MockAudioCapture::start(const AudioFormat& format) {
    auto valid = validate_format(format);
    if (!valid.ok()) return valid;
    std::lock_guard<std::mutex> lock(mutex_);
    if (used_) return {protocol::StatusCode::INVALID_STATE, "capture is single-use"};
    used_ = true;
    format_ = format;
    state_ = AudioDeviceState::RUNNING;
    return protocol::Status::Ok();
}

AudioCaptureResult MockAudioCapture::read(std::chrono::milliseconds timeout) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != AudioDeviceState::RUNNING)
            return {{used_ ? protocol::StatusCode::CANCELLED : protocol::StatusCode::INVALID_STATE,
                     "capture stopped"}, {}};
    }
    PcmBuffer buffer;
    const auto result = fixture_.pop_for(buffer, timeout);
    if (result == ipc::QueueStatus::OK) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != AudioDeviceState::RUNNING)
            return {{protocol::StatusCode::CANCELLED, "capture stopped"}, {}};
        return {protocol::Status::Ok(), std::move(buffer)};
    }
    if (result == ipc::QueueStatus::CLOSED)
        return {{protocol::StatusCode::CANCELLED, "capture stopped"}, {}};
    return {{protocol::StatusCode::TIMEOUT, "no fixture"}, {}};
}

void MockAudioCapture::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = AudioDeviceState::STOPPED;
    fixture_.close();
}

AudioDeviceState MockAudioCapture::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::optional<AudioFormat> MockAudioCapture::actual_format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != AudioDeviceState::RUNNING) return std::nullopt;
    return format_;
}

protocol::Status MockAudioCapture::push_fixture(PcmBuffer buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != AudioDeviceState::RUNNING || !same_format(format_, buffer.format))
        return {protocol::StatusCode::INVALID_STATE, "capture format or state"};
    if (buffer.bytes.size() > 1024 * 1024 ||
        buffer.bytes.size() % (format_.channels * bytes_per_sample(format_.sample_format)) != 0)
        return {protocol::StatusCode::INVALID_ARGUMENT, "PCM length"};
    const auto result = fixture_.try_push(std::move(buffer));
    return result == ipc::QueueStatus::OK ? protocol::Status::Ok()
                                           : protocol::Status{protocol::StatusCode::UNAVAILABLE, "capture queue full"};
}

protocol::Status MockAudioPlayback::start(const AudioFormat& format) {
    auto valid = validate_format(format);
    if (!valid.ok() || capacity_ == 0) return {protocol::StatusCode::INVALID_ARGUMENT, "playback format/capacity"};
    std::lock_guard<std::mutex> lock(mutex_);
    if (used_) return {protocol::StatusCode::INVALID_STATE, "playback is single-use"};
    used_ = true;
    format_ = format;
    state_ = AudioDeviceState::RUNNING;
    return protocol::Status::Ok();
}

AudioPlaybackResult MockAudioPlayback::play(const PcmBuffer& buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer.session_id == 0 || buffer.session_id <= cancelled_through_)
        return {{protocol::StatusCode::CANCELLED, "stale playback session"}, 0};
    if (state_ != AudioDeviceState::RUNNING || !same_format(format_, buffer.format))
        return {{protocol::StatusCode::INVALID_STATE, "playback format or state"}, 0};
    const auto frame_size = format_.channels * bytes_per_sample(format_.sample_format);
    if (buffer.bytes.empty() || buffer.bytes.size() > 1024 * 1024 || buffer.bytes.size() % frame_size != 0)
        return {{protocol::StatusCode::INVALID_ARGUMENT, "PCM length"}, 0};
    if (played_.size() == capacity_) return {{protocol::StatusCode::UNAVAILABLE, "playback full"}, 0};
    played_.push_back(buffer);
    return {protocol::Status::Ok(), buffer.bytes.size() / frame_size};
}

void MockAudioPlayback::cancel(protocol::SessionId session) {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_through_ = std::max(cancelled_through_, session);
    played_.erase(std::remove_if(played_.begin(), played_.end(), [session](const PcmBuffer& b) {
        return b.session_id <= session;
    }), played_.end());
}

void MockAudioPlayback::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = AudioDeviceState::STOPPED;
    played_.clear();
}

AudioDeviceState MockAudioPlayback::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::size_t MockAudioPlayback::played_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return played_.size();
}

}  // namespace cockpit::audio
