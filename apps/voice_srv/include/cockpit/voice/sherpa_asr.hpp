#pragma once

#include "cockpit/voice/voice.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cockpit::voice {

struct SherpaModelFiles {
    std::filesystem::path encoder;
    std::filesystem::path decoder;
    std::filesystem::path joiner;
    std::filesystem::path tokens;
    std::filesystem::path bpe;
};

// The small ASR-01 manifest is line-oriented key=value; hashes record provenance
// but are not runtime verified. Paths are supplied by the caller, never fixed here.
protocol::Status read_sherpa_model_manifest(const std::filesystem::path& manifest,
                                            const std::filesystem::path& model_dir,
                                            SherpaModelFiles& files);

struct SherpaDecodeResult {
    protocol::Status status;
    std::string text;
};

// Adapter seam: production C API implementation is built only when enabled;
// tests inject a fake. One backend owns one recognizer and at most one stream.
class ISherpaEngine {
public:
    virtual ~ISherpaEngine() = default;
    virtual protocol::Status load(const SherpaModelFiles& files) = 0;
    virtual void unload() = 0;
    virtual protocol::Status start_stream() = 0;
    virtual SherpaDecodeResult decode(const float* samples, std::size_t count, bool input_finished) = 0;
    virtual void stop_stream() = 0;
};

class SherpaAsrBackend final : public IAsrBackend {
public:
    explicit SherpaAsrBackend(std::unique_ptr<ISherpaEngine> engine);
    ~SherpaAsrBackend() override;
    protocol::Status load(const SherpaModelFiles& files);
    void unload();
    bool loaded() const;
    protocol::Status start_session(SessionToken token, const audio::AudioFormat& format,
                                   AsrCallback callback) override;
    protocol::Status push_audio(const audio::PcmBuffer& buffer) override;
    protocol::Status finish_input(SessionToken token) override;
    void cancel(SessionToken token) override;

private:
    void emit(AsrEventType type, const std::string& text, protocol::Status status);
    std::unique_ptr<ISherpaEngine> engine_;
    mutable std::mutex mutex_;
    bool loaded_{false};
    bool active_{false};
    SessionToken token_;
    AsrCallback callback_;
    std::string last_partial_;
    std::uint64_t sequence_{0};
};

// Defined only in the optional Sherpa-linked target.
std::unique_ptr<ISherpaEngine> make_sherpa_c_api_engine();

}  // namespace cockpit::voice
