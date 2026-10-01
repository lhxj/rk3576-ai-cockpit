#pragma once

#include "cockpit/voice/vad.hpp"

namespace cockpit::voice {

// Optional target; uses only the fixed v1.11.3 C API, with one owned model.
class SherpaVadBackend final : public IVadBackend {
public:
    SherpaVadBackend() = default;
    ~SherpaVadBackend() override;
    protocol::Status configure(const VadConfig& config) override;
    void reset() override;
    VadAcceptResult accept_samples(const audio::PcmBuffer& pcm,
                                   std::chrono::steady_clock::time_point captured_at) override;
    VadAcceptResult flush() override;
    VadState state() const override { return state_; }
    std::uint32_t load_count() const { return load_count_; }

private:
    struct Handle;
    Handle* handle_{nullptr};
    VadConfig config_;
    VadState state_{VadState::Silence};
    std::uint64_t samples_seen_{0};
    std::uint32_t load_count_{0};
    VadAcceptResult collect(std::chrono::steady_clock::time_point at);
};

}  // namespace cockpit::voice
