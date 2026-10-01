#include "cockpit/audio/wav_reader.hpp"
#include "cockpit/voice/sherpa_asr.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    cockpit::voice::SherpaModelFiles files;
    auto status = cockpit::voice::read_sherpa_model_manifest(argv[1], argv[2], files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 3; }
    auto wav = cockpit::audio::read_pcm_wav(argv[3]);
    if (!wav.status.ok()) { std::cerr << wav.status.detail << '\n'; return 4; }
    cockpit::voice::SherpaAsrBackend backend(cockpit::voice::make_sherpa_c_api_engine());
    status = backend.load(files);
    if (!status.ok()) { std::cerr << status.detail << '\n'; return 5; }
    cockpit::voice::VoiceSessionController controller(71);
    int old_finals = 0, new_finals = 0;
    auto old = controller.start(101);
    assert(controller.transition(old, cockpit::voice::VoiceSessionState::Recognizing).ok());
    status = backend.start_session(old, wav.pcm.format, [&](const cockpit::voice::AsrEvent& event) {
        if (event.type == cockpit::voice::AsrEventType::FINAL)
            controller.deliver_event(event.token, cockpit::protocol::MessageType::ASR_FINAL,
                                     [&] { ++old_finals; });
    });
    if (!status.ok()) return 6;
    cockpit::audio::PcmBuffer first = wav.pcm;
    first.session_id = old.session_id;
    if (first.bytes.size() > 32000) first.bytes.resize(32000);  // one second maximum
    if (!backend.push_audio(first).ok()) return 7;
    if (!controller.cancel(old).ok()) return 8;
    backend.cancel(old);
    if (backend.finish_input(old).ok() || old_finals != 0) return 9;
    if (!controller.complete_cancel(old).ok()) return 10;

    const auto current = controller.start(102);
    if (!controller.transition(current, cockpit::voice::VoiceSessionState::Recognizing).ok()) return 11;
    wav.pcm.session_id = current.session_id;
    status = backend.start_session(current, wav.pcm.format, [&](const cockpit::voice::AsrEvent& event) {
        if (event.type == cockpit::voice::AsrEventType::FINAL)
            controller.deliver_event(event.token, cockpit::protocol::MessageType::ASR_FINAL, [&] {
                if (!event.text.empty()) ++new_finals;
            });
    });
    if (!status.ok() || !backend.push_audio(wav.pcm).ok() || !backend.finish_input(current).ok()) return 12;
    if (old_finals != 0 || new_finals != 1) return 13;
    std::cout << "SHERPA_CANCEL_SESSION_PASS\n";
    return 0;
}
