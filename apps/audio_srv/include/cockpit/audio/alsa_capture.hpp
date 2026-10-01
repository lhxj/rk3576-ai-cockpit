#pragma once

#include "cockpit/audio/audio.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace cockpit::audio {

struct AlsaCaptureMetrics {
    std::uint64_t captured_frames{0};
    std::uint64_t xrun_count{0};
    std::uint32_t period_frames{0};
    std::uint32_t buffer_frames{0};
    std::uint32_t peak_abs_sample{0};
    std::uint64_t nonzero_samples{0};
    std::uint64_t sample_square_sum{0};
};

// Only audio_srv links libasound. One instance owns one capture PCM handle.
class AlsaAudioCapture final : public IAudioCapture {
public:
    explicit AlsaAudioCapture(std::string device);
    ~AlsaAudioCapture() override;
    AlsaAudioCapture(const AlsaAudioCapture&) = delete;
    AlsaAudioCapture& operator=(const AlsaAudioCapture&) = delete;

    protocol::Status start(const AudioFormat& format) override;
    AudioCaptureResult read(std::chrono::milliseconds timeout) override;
    void stop() override;
    AudioDeviceState state() const override;
    std::optional<AudioFormat> actual_format() const override;
    AlsaCaptureMetrics metrics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpit::audio
