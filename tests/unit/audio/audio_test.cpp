#include "cockpit/audio/audio.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x << '\n'; return 1; } } while (false)

int main() {
    using namespace cockpit;
    using namespace std::chrono_literals;
    audio::AudioFormat format;
    audio::MockAudioCapture capture(2);
    CHECK(capture.start(format).ok());
    audio::PcmBuffer pcm{format, 100, {0, 0, 1, 0}};
    CHECK(capture.push_fixture(pcm).ok());
    auto read = capture.read(10ms);
    CHECK(read.status.ok() && read.buffer.bytes == pcm.bytes);
    protocol::StatusCode blocked_result = protocol::StatusCode::OK;
    std::thread blocked([&] { blocked_result = capture.read(5s).status.code; });
    capture.stop();
    blocked.join();
    CHECK(blocked_result == protocol::StatusCode::CANCELLED);
    CHECK(capture.state() == audio::AudioDeviceState::STOPPED);

    audio::MockAudioPlayback playback(2);
    CHECK(playback.start(format).ok());
    CHECK(playback.play(pcm).frames_accepted == 2);
    CHECK(playback.play(pcm).frames_accepted == 2);
    CHECK(playback.play(pcm).status.code == protocol::StatusCode::UNAVAILABLE);
    playback.cancel(100);
    CHECK(playback.played_count() == 0);
    CHECK(playback.play(pcm).status.code == protocol::StatusCode::CANCELLED);
    pcm.session_id = 101;
    CHECK(playback.play(pcm).status.ok());
    playback.stop();
    CHECK(playback.state() == audio::AudioDeviceState::STOPPED);
    CHECK(playback.play(pcm).status.code == protocol::StatusCode::INVALID_STATE);
    return 0;
}
