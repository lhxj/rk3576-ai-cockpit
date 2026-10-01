#include "cockpit/voice/sherpa_asr.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <utility>

namespace cockpit::voice {
namespace {
using protocol::Status;
using protocol::StatusCode;

bool valid_name(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}
Status validate_files(const SherpaModelFiles& files) {
    const std::array<std::filesystem::path, 4> required{files.encoder, files.decoder, files.joiner, files.tokens};
    for (const auto& path : required) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
            return {StatusCode::UNAVAILABLE, "MODEL_NOT_FOUND: " + path.string()};
    }
    return Status::Ok();
}
bool same_token(SessionToken a, SessionToken b) {
    return a.session_id == b.session_id && a.generation == b.generation &&
           a.request_id == b.request_id && a.boot_epoch == b.boot_epoch;
}
}  // namespace

Status read_sherpa_model_manifest(const std::filesystem::path& manifest,
                                  const std::filesystem::path& model_dir, SherpaModelFiles& files) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(manifest, ec) || std::filesystem::file_size(manifest, ec) > 8192)
        return {StatusCode::UNAVAILABLE, "model manifest missing or too large"};
    std::ifstream in(manifest);
    if (!in) return {StatusCode::UNAVAILABLE, "model manifest open failed"};
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos || equal == 0 || equal == line.size() - 1)
            return {StatusCode::MALFORMED, "model manifest syntax"};
        if (!values.emplace(line.substr(0, equal), line.substr(equal + 1)).second)
            return {StatusCode::MALFORMED, "duplicate model manifest key"};
    }
    if (!in.eof()) return {StatusCode::MALFORMED, "model manifest read error"};
    if (values["model_type"] != "online_zipformer_transducer" || values["sample_rate"] != "16000" ||
        values["channels"] != "1" || values["precision"] != "mixed_int8_fp32")
        return {StatusCode::INVALID_ARGUMENT, "unsupported ASR model manifest"};
    for (const auto* key : {"encoder", "decoder", "joiner", "tokens", "bpe"}) {
        if (!valid_name(values[key])) return {StatusCode::MALFORMED, "invalid model filename"};
    }
    files.encoder = model_dir / values["encoder"];
    files.decoder = model_dir / values["decoder"];
    files.joiner = model_dir / values["joiner"];
    files.tokens = model_dir / values["tokens"];
    files.bpe = model_dir / values["bpe"];
    return validate_files(files);
}

SherpaAsrBackend::SherpaAsrBackend(std::unique_ptr<ISherpaEngine> engine) : engine_(std::move(engine)) {}
SherpaAsrBackend::~SherpaAsrBackend() { unload(); }

Status SherpaAsrBackend::load(const SherpaModelFiles& files) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!engine_) return {StatusCode::INVALID_STATE, "Sherpa engine absent"};
    if (active_) return {StatusCode::INVALID_STATE, "ASR session active"};
    if (loaded_) return Status::Ok();
    auto status = validate_files(files);
    if (!status.ok()) return status;
    status = engine_->load(files);
    if (!status.ok()) return status;
    loaded_ = true;
    return Status::Ok();
}
void SherpaAsrBackend::unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!engine_) return;
    if (active_) engine_->stop_stream();
    active_ = false;
    callback_ = {};
    if (loaded_) engine_->unload();
    loaded_ = false;
}
bool SherpaAsrBackend::loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
}
Status SherpaAsrBackend::start_session(SessionToken token, const audio::AudioFormat& format, AsrCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!loaded_) return {StatusCode::UNAVAILABLE, "ASR model not loaded"};
    if (active_) return {StatusCode::INVALID_STATE, "ASR session active"};
    if (token.session_id == 0 || token.generation == 0 || token.request_id == 0 || token.boot_epoch == 0 || !callback)
        return {StatusCode::INVALID_ARGUMENT, "invalid ASR session token/callback"};
    if (format.sample_rate != 16000 || format.channels != 1 ||
        format.sample_format != audio::SampleFormat::S16_LE || format.frames_per_buffer == 0)
        return {StatusCode::INVALID_ARGUMENT, "UNSUPPORTED_AUDIO_FORMAT: require PCM mono S16 16000 Hz"};
    auto status = engine_->start_stream();
    if (!status.ok()) return status;
    token_ = token;
    callback_ = std::move(callback);
    last_partial_.clear();
    sequence_ = 0;
    active_ = true;
    return Status::Ok();
}
void SherpaAsrBackend::emit(AsrEventType type, const std::string& text, Status status) {
    if (active_ && callback_) callback_({type, token_, ++sequence_, text, std::move(status)});
}
Status SherpaAsrBackend::push_audio(const audio::PcmBuffer& buffer) {
    SessionToken started;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_) return {StatusCode::CANCELLED, "ASR session inactive"};
        if (buffer.session_id != token_.session_id || buffer.format.sample_rate != 16000 ||
            buffer.format.channels != 1 || buffer.format.sample_format != audio::SampleFormat::S16_LE ||
            buffer.bytes.empty() || (buffer.bytes.size() & 1U))
            return {StatusCode::INVALID_ARGUMENT, "invalid ASR PCM buffer"};
        started = token_;
    }
    constexpr std::size_t kMaxChunkSamples = 16000;
    for (std::size_t at = 0; at < buffer.bytes.size();) {
        const auto count = std::min(kMaxChunkSamples, (buffer.bytes.size() - at) / 2U);
        std::vector<float> samples(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto raw = static_cast<std::uint16_t>(buffer.bytes[at + i * 2] |
                             (static_cast<std::uint16_t>(buffer.bytes[at + i * 2 + 1]) << 8));
            const auto signed_sample = raw < 32768U ? static_cast<std::int32_t>(raw) :
                static_cast<std::int32_t>(raw) - 65536;
            samples[i] = static_cast<float>(signed_sample) / 32768.0f;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || !same_token(started, token_))
            return {StatusCode::CANCELLED, "ASR session cancelled"};
        auto decoded = engine_->decode(samples.data(), count, false);
        if (!decoded.status.ok()) {
            emit(AsrEventType::ERROR, {}, decoded.status);
            engine_->stop_stream();
            active_ = false;
            callback_ = {};
            return decoded.status;
        }
        if (!decoded.text.empty() && decoded.text != last_partial_) {
            last_partial_ = decoded.text;
            emit(AsrEventType::PARTIAL, decoded.text, Status::Ok());
        }
        at += count * 2U;
    }
    return Status::Ok();
}
Status SherpaAsrBackend::finish_input(SessionToken token) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !same_token(token, token_)) return {StatusCode::INVALID_STATE, "ASR session mismatch"};
    auto result = engine_->decode(nullptr, 0, true);
    if (result.status.ok()) emit(AsrEventType::FINAL, result.text, Status::Ok());
    else emit(AsrEventType::ERROR, {}, result.status);
    engine_->stop_stream();
    active_ = false;
    callback_ = {};
    return result.status;
}
void SherpaAsrBackend::cancel(SessionToken token) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !same_token(token, token_)) return;
    engine_->stop_stream();
    active_ = false;
    callback_ = {};
}

}  // namespace cockpit::voice
