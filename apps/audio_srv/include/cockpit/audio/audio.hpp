#pragma once

#include "cockpit/ipc/bounded_queue.hpp"
#include "cockpit/protocol/message.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace cockpit::audio {

enum class SampleFormat { S16_LE, F32_LE };
enum class AudioDeviceState { STOPPED, RUNNING, ERROR };

struct AudioFormat {
    std::uint32_t sample_rate{16000};
    std::uint16_t channels{1};
    SampleFormat sample_format{SampleFormat::S16_LE};
    std::uint32_t frames_per_buffer{320};
};

struct PcmBuffer {
    AudioFormat format;
    protocol::SessionId session_id{0};
    std::vector<std::uint8_t> bytes;
};

struct AudioCaptureResult {
    protocol::Status status;
    PcmBuffer buffer;
};

struct AudioPlaybackResult {
    protocol::Status status;
    std::size_t frames_accepted{0};
};

protocol::Status validate_format(const AudioFormat& format);

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;
    virtual protocol::Status start(const AudioFormat& format) = 0;
    virtual AudioCaptureResult read(std::chrono::milliseconds timeout) = 0;
    virtual void stop() = 0;
    virtual AudioDeviceState state() const = 0;
};

class IAudioPlayback {
public:
    virtual ~IAudioPlayback() = default;
    virtual protocol::Status start(const AudioFormat& format) = 0;
    virtual AudioPlaybackResult play(const PcmBuffer& buffer) = 0;
    virtual void cancel(protocol::SessionId session) = 0;
    virtual void stop() = 0;
    virtual AudioDeviceState state() const = 0;
};

// Host fixtures only. The real ALSA backend will live exclusively in audio_srv.
class MockAudioCapture final : public IAudioCapture {
public:
    explicit MockAudioCapture(std::size_t capacity) : fixture_(capacity) {}
    protocol::Status start(const AudioFormat& format) override;
    AudioCaptureResult read(std::chrono::milliseconds timeout) override;
    void stop() override;
    AudioDeviceState state() const override;
    protocol::Status push_fixture(PcmBuffer buffer);

private:
    mutable std::mutex mutex_;
    AudioDeviceState state_{AudioDeviceState::STOPPED};
    bool used_{false};
    AudioFormat format_;
    ipc::BoundedQueue<PcmBuffer> fixture_;
};

class MockAudioPlayback final : public IAudioPlayback {
public:
    explicit MockAudioPlayback(std::size_t capacity) : capacity_(capacity) {}
    protocol::Status start(const AudioFormat& format) override;
    AudioPlaybackResult play(const PcmBuffer& buffer) override;
    void cancel(protocol::SessionId session) override;
    void stop() override;
    AudioDeviceState state() const override;
    std::size_t played_count() const;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    AudioDeviceState state_{AudioDeviceState::STOPPED};
    bool used_{false};
    AudioFormat format_;
    std::vector<PcmBuffer> played_;
    protocol::SessionId cancelled_through_{0};
};

}  // namespace cockpit::audio
