#include "cockpit/voice/sherpa_asr.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
using cockpit::protocol::Status;
using cockpit::protocol::StatusCode;
using cockpit::voice::SherpaDecodeResult;

struct Counts { int loads{0}; int unloads{0}; int starts{0}; int stops{0}; int decodes{0}; };
class FakeEngine final : public cockpit::voice::ISherpaEngine {
public:
    explicit FakeEngine(Counts& counts) : counts_(counts) {}
    Status load(const cockpit::voice::SherpaModelFiles&) override { ++counts_.loads; return Status::Ok(); }
    void unload() override { ++counts_.unloads; }
    Status start_stream() override { ++counts_.starts; return Status::Ok(); }
    SherpaDecodeResult decode(const float*, std::size_t, bool finish) override {
        ++counts_.decodes;
        return {Status::Ok(), finish ? "最终文本" : "部分文本"};
    }
    void stop_stream() override { ++counts_.stops; }
private:
    Counts& counts_;
};
void touch(const std::filesystem::path& p) { std::ofstream(p).put('x'); }
}  // namespace

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() /
        ("cockpit-asr-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(dir);
    for (const auto* name : {"encoder.onnx", "decoder.onnx", "joiner.onnx", "tokens.txt", "bpe.model"}) touch(dir / name);
    const auto manifest = dir / "model.conf";
    {
        std::ofstream out(manifest);
        out << "model_type=online_zipformer_transducer\nsample_rate=16000\nchannels=1\nprecision=mixed_int8_fp32\n"
               "encoder=encoder.onnx\ndecoder=decoder.onnx\njoiner=joiner.onnx\ntokens=tokens.txt\nbpe=bpe.model\n";
    }
    cockpit::voice::SherpaModelFiles files;
    assert(cockpit::voice::read_sherpa_model_manifest(manifest, dir, files).ok());
    assert(!cockpit::voice::read_sherpa_model_manifest(manifest, dir / "missing", files).ok());
    fs::remove(dir / "encoder.onnx");
    assert(!cockpit::voice::read_sherpa_model_manifest(manifest, dir, files).ok());
    touch(dir / "encoder.onnx");
    assert(cockpit::voice::read_sherpa_model_manifest(manifest, dir, files).ok());

    Counts counts;
    cockpit::voice::SherpaAsrBackend backend(std::make_unique<FakeEngine>(counts));
    cockpit::voice::VoiceSessionController controller(77);
    assert(backend.load(files).ok());
    assert(backend.load(files).ok() && counts.loads == 1);
    int partials = 0, finals = 0;
    auto token = controller.start(1);
    assert(controller.transition(token, cockpit::voice::VoiceSessionState::Recognizing).ok());
    auto callback = [&](const cockpit::voice::AsrEvent& event) {
        const auto kind = event.type == cockpit::voice::AsrEventType::FINAL ?
            cockpit::protocol::MessageType::ASR_FINAL : cockpit::protocol::MessageType::ASR_PARTIAL;
        controller.deliver_event(event.token, kind, [&] {
            if (event.type == cockpit::voice::AsrEventType::FINAL) { ++finals; assert(event.text == "最终文本"); }
            else ++partials;
        });
    };
    cockpit::audio::AudioFormat format;
    cockpit::audio::PcmBuffer buffer;
    buffer.format = format;
    buffer.session_id = token.session_id;
    buffer.bytes = {0, 0, 255, 127};
    assert(backend.start_session(token, format, callback).ok());
    assert(backend.push_audio(buffer).ok());
    assert(backend.finish_input(token).ok());
    assert(partials == 1 && finals == 1 && counts.loads == 1);

    auto old = token;
    token = controller.start(2);
    assert(controller.transition(token, cockpit::voice::VoiceSessionState::Recognizing).ok());
    assert(!controller.deliver_event(old, cockpit::protocol::MessageType::ASR_FINAL, [&] { ++finals; }).ok());
    buffer.session_id = token.session_id;
    assert(backend.start_session(token, format, callback).ok());
    backend.cancel(old);  // An old token cannot terminate the new stream.
    assert(backend.push_audio(buffer).ok());
    assert(controller.cancel(token).ok());
    backend.cancel(token);
    assert(backend.finish_input(token).code == StatusCode::INVALID_STATE);
    assert(finals == 1);
    assert(controller.complete_cancel(token).ok());

    token = controller.start(3);
    assert(controller.transition(token, cockpit::voice::VoiceSessionState::Recognizing).ok());
    buffer.session_id = token.session_id;
    assert(backend.start_session(token, format, callback).ok());
    assert(backend.push_audio(buffer).ok());
    assert(backend.finish_input(token).ok());
    assert(finals == 2 && counts.loads == 1 && counts.starts == 3);
    backend.unload();
    assert(!backend.loaded() && counts.unloads == 1);
    assert(backend.load(files).ok() && counts.loads == 2);
    backend.unload();
    assert(counts.unloads == 2);
    fs::remove_all(dir);
}
