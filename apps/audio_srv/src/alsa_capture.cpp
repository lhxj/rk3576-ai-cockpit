#include "cockpit/audio/alsa_capture.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>
#include <vector>

namespace cockpit::audio {
namespace {
using protocol::Status;
using protocol::StatusCode;
Status alsa_error(const char* op, int code) {
    return {code == -EBUSY ? StatusCode::UNAVAILABLE : StatusCode::INTERNAL_ERROR,
            std::string(code == -EBUSY ? "AUDIO_DEVICE_BUSY: " : "ALSA: ") + op + ": " + snd_strerror(code)};
}
}  // namespace

struct AlsaAudioCapture::Impl {
    explicit Impl(std::string name) : device(std::move(name)) {}
    std::string device;
    mutable std::mutex mutex;
    std::atomic<bool> stopping{false};
    snd_pcm_t* pcm{nullptr};
    AudioFormat format;
    AudioDeviceState state{AudioDeviceState::STOPPED};
    AlsaCaptureMetrics counters;
};

AlsaAudioCapture::AlsaAudioCapture(std::string device) : impl_(std::make_unique<Impl>(std::move(device))) {}
AlsaAudioCapture::~AlsaAudioCapture() { stop(); }

Status AlsaAudioCapture::start(const AudioFormat& requested) {
    auto valid = validate_format(requested);
    if (!valid.ok()) return valid;
    if (requested.sample_rate != 16000 || requested.channels != 1 ||
        requested.sample_format != SampleFormat::S16_LE)
        return {StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: require 16000/1/S16_LE"};
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->pcm || impl_->state == AudioDeviceState::RUNNING)
        return {StatusCode::INVALID_STATE, "capture already running"};
    if (impl_->device.empty()) return {StatusCode::INVALID_ARGUMENT, "ALSA device empty"};
    impl_->stopping = false;
    int rc = snd_pcm_open(&impl_->pcm, impl_->device.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (rc < 0) { impl_->pcm = nullptr; return alsa_error("open", rc); }
    auto fail = [&](Status status) {
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
        impl_->state = AudioDeviceState::ERROR;
        return status;
    };
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca(&hw);
    if ((rc = snd_pcm_hw_params_any(impl_->pcm, hw)) < 0 ||
        (rc = snd_pcm_hw_params_set_access(impl_->pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ||
        (rc = snd_pcm_hw_params_set_format(impl_->pcm, hw, SND_PCM_FORMAT_S16_LE)) < 0 ||
        (rc = snd_pcm_hw_params_set_channels(impl_->pcm, hw, 1)) < 0)
        return fail(alsa_error("set hardware format", rc));
    unsigned rate = 16000;
    int direction = 0;
    if ((rc = snd_pcm_hw_params_set_rate_near(impl_->pcm, hw, &rate, &direction)) < 0)
        return fail(alsa_error("set rate", rc));
    snd_pcm_uframes_t period = requested.frames_per_buffer;
    direction = 0;
    if ((rc = snd_pcm_hw_params_set_period_size_near(impl_->pcm, hw, &period, &direction)) < 0)
        return fail(alsa_error("set period", rc));
    snd_pcm_uframes_t buffer = period * 4;
    if ((rc = snd_pcm_hw_params_set_buffer_size_near(impl_->pcm, hw, &buffer)) < 0 ||
        (rc = snd_pcm_hw_params(impl_->pcm, hw)) < 0)
        return fail(alsa_error("commit hardware format", rc));
    unsigned actual_rate = 0, actual_channels = 0;
    snd_pcm_format_t actual_sample = SND_PCM_FORMAT_UNKNOWN;
    if (snd_pcm_hw_params_get_rate(hw, &actual_rate, &direction) < 0 ||
        snd_pcm_hw_params_get_channels(hw, &actual_channels) < 0 ||
        snd_pcm_hw_params_get_format(hw, &actual_sample) < 0 ||
        snd_pcm_hw_params_get_period_size(hw, &period, &direction) < 0 ||
        snd_pcm_hw_params_get_buffer_size(hw, &buffer) < 0)
        return fail({StatusCode::INTERNAL_ERROR, "ALSA negotiated parameters unavailable"});
    if (actual_rate != 16000 || actual_channels != 1 || actual_sample != SND_PCM_FORMAT_S16_LE ||
        period == 0 || period > 16000 || period > std::numeric_limits<std::uint32_t>::max())
        return fail({StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: ALSA negotiation differed"});
    if ((rc = snd_pcm_prepare(impl_->pcm)) < 0) return fail(alsa_error("prepare", rc));
    // Nonblocking capture must be started before waiting for the first period.
    if ((rc = snd_pcm_start(impl_->pcm)) < 0) return fail(alsa_error("start", rc));
    impl_->format = {actual_rate, static_cast<std::uint16_t>(actual_channels), SampleFormat::S16_LE,
                     static_cast<std::uint32_t>(period)};
    impl_->counters = {0, 0, static_cast<std::uint32_t>(period), static_cast<std::uint32_t>(buffer)};
    impl_->state = AudioDeviceState::RUNNING;
    return Status::Ok();
}

AudioCaptureResult AlsaAudioCapture::read(std::chrono::milliseconds timeout) {
    if (timeout.count() < 0) return {{StatusCode::INVALID_ARGUMENT, "negative capture timeout"}, {}};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        if (impl_->stopping) return {{StatusCode::CANCELLED, "capture stopped"}, {}};
        std::unique_lock<std::mutex> lock(impl_->mutex);
        if (!impl_->pcm || impl_->state != AudioDeviceState::RUNNING)
            return {{StatusCode::CANCELLED, "capture stopped"}, {}};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) return {{StatusCode::TIMEOUT, "capture wait"}, {}};
        const int wait_ms = static_cast<int>(std::min<std::int64_t>(remaining, 50));
        int rc = snd_pcm_wait(impl_->pcm, wait_ms);
        if (impl_->stopping) return {{StatusCode::CANCELLED, "capture stopped"}, {}};
        if (rc == 0 || rc == -EAGAIN || rc == -EINTR) continue;
        if (rc > 0) {
            PcmBuffer pcm;
            pcm.format = impl_->format;
            pcm.bytes.resize(static_cast<std::size_t>(impl_->format.frames_per_buffer) * 2U);
            const snd_pcm_sframes_t frames = snd_pcm_readi(impl_->pcm, pcm.bytes.data(),
                                                           impl_->format.frames_per_buffer);
            if (frames > 0) {
                pcm.bytes.resize(static_cast<std::size_t>(frames) * 2U);
                impl_->counters.captured_frames += static_cast<std::uint64_t>(frames);
                for (std::size_t i = 0; i < pcm.bytes.size(); i += 2) {
                    const auto raw = static_cast<std::uint16_t>(pcm.bytes[i] |
                        (static_cast<std::uint16_t>(pcm.bytes[i + 1]) << 8));
                    const auto signed_sample = raw < 32768U ? static_cast<std::int32_t>(raw) :
                        static_cast<std::int32_t>(raw) - 65536;
                    const auto magnitude = static_cast<std::uint32_t>(signed_sample < 0 ?
                        -signed_sample : signed_sample);
                    impl_->counters.peak_abs_sample =
                        std::max(impl_->counters.peak_abs_sample, magnitude);
                    impl_->counters.nonzero_samples += magnitude != 0;
                    impl_->counters.sample_square_sum += static_cast<std::uint64_t>(magnitude) * magnitude;
                }
                return {Status::Ok(), std::move(pcm)};
            }
            rc = static_cast<int>(frames);
            if (rc == -EAGAIN || rc == -EINTR) continue;
        }
        if (rc == -EPIPE || rc == -ESTRPIPE) {
            ++impl_->counters.xrun_count;
            if (rc == -ESTRPIPE) {
                rc = snd_pcm_resume(impl_->pcm);
                if (rc == -EAGAIN) rc = snd_pcm_prepare(impl_->pcm);
            } else rc = snd_pcm_prepare(impl_->pcm);
            if (rc >= 0) continue;
        }
        impl_->state = AudioDeviceState::ERROR;
        return {alsa_error("capture read/recover", rc), {}};
    }
}

void AlsaAudioCapture::stop() {
    impl_->stopping = true;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->pcm) {
        snd_pcm_drop(impl_->pcm);
        snd_pcm_close(impl_->pcm);
        impl_->pcm = nullptr;
    }
    impl_->state = AudioDeviceState::STOPPED;
}
AudioDeviceState AlsaAudioCapture::state() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state;
}
std::optional<AudioFormat> AlsaAudioCapture::actual_format() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->pcm || impl_->state != AudioDeviceState::RUNNING) return std::nullopt;
    return impl_->format;
}
AlsaCaptureMetrics AlsaAudioCapture::metrics() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->counters;
}

}  // namespace cockpit::audio
