#include "cockpit/voice/sherpa_asr.hpp"

#include "sherpa-onnx/c-api/c-api.h"

#include <memory>
#include <string>

namespace cockpit::voice {
namespace {
class SherpaCApiEngine final : public ISherpaEngine {
public:
    ~SherpaCApiEngine() override { unload(); }
    protocol::Status load(const SherpaModelFiles& files) override {
        if (recognizer_) return protocol::Status::Ok();
        const auto encoder = files.encoder.string();
        const auto decoder = files.decoder.string();
        const auto joiner = files.joiner.string();
        const auto tokens = files.tokens.string();
        SherpaOnnxOnlineRecognizerConfig config{};
        config.feat_config.sample_rate = 16000;
        config.feat_config.feature_dim = 80;
        config.model_config.transducer.encoder = encoder.c_str();
        config.model_config.transducer.decoder = decoder.c_str();
        config.model_config.transducer.joiner = joiner.c_str();
        config.model_config.tokens = tokens.c_str();
        config.model_config.num_threads = 1;
        config.model_config.provider = "cpu";
        config.decoding_method = "greedy_search";
        config.max_active_paths = 4;
        config.enable_endpoint = 0;
        recognizer_ = SherpaOnnxCreateOnlineRecognizer(&config);
        if (!recognizer_) return {protocol::StatusCode::UNAVAILABLE, "MODEL_LOAD_FAILED: Sherpa recognizer"};
        return protocol::Status::Ok();
    }
    void unload() override {
        stop_stream();
        if (recognizer_) SherpaOnnxDestroyOnlineRecognizer(recognizer_);
        recognizer_ = nullptr;
    }
    protocol::Status start_stream() override {
        if (!recognizer_ || stream_) return {protocol::StatusCode::INVALID_STATE, "Sherpa stream state"};
        stream_ = SherpaOnnxCreateOnlineStream(recognizer_);
        if (!stream_) return {protocol::StatusCode::INTERNAL_ERROR, "Sherpa stream creation failed"};
        return protocol::Status::Ok();
    }
    SherpaDecodeResult decode(const float* samples, std::size_t count, bool input_finished) override {
        if (!recognizer_ || !stream_) return {{protocol::StatusCode::INVALID_STATE, "Sherpa stream absent"}, {}};
        if (count > 16000 || (count != 0 && !samples))
            return {{protocol::StatusCode::INVALID_ARGUMENT, "Sherpa chunk outside limit"}, {}};
        if (count != 0) SherpaOnnxOnlineStreamAcceptWaveform(stream_, 16000, samples, static_cast<int32_t>(count));
        if (input_finished) SherpaOnnxOnlineStreamInputFinished(stream_);
        std::size_t steps = 0;
        while (SherpaOnnxIsOnlineStreamReady(recognizer_, stream_)) {
            if (++steps > 100000) return {{protocol::StatusCode::INTERNAL_ERROR, "Sherpa decode limit"}, {}};
            SherpaOnnxDecodeOnlineStream(recognizer_, stream_);
        }
        const auto* result = SherpaOnnxGetOnlineStreamResult(recognizer_, stream_);
        if (!result) return {{protocol::StatusCode::INTERNAL_ERROR, "Sherpa result unavailable"}, {}};
        std::string text = result->text ? result->text : "";
        SherpaOnnxDestroyOnlineRecognizerResult(result);
        return {protocol::Status::Ok(), std::move(text)};
    }
    void stop_stream() override {
        if (stream_) SherpaOnnxDestroyOnlineStream(stream_);
        stream_ = nullptr;
    }
private:
    const SherpaOnnxOnlineRecognizer* recognizer_{nullptr};
    const SherpaOnnxOnlineStream* stream_{nullptr};
};
}  // namespace

std::unique_ptr<ISherpaEngine> make_sherpa_c_api_engine() {
    return std::make_unique<SherpaCApiEngine>();
}
}  // namespace cockpit::voice
