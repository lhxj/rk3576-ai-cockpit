#pragma once

#include "cockpit/infer/infer.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace cockpit::infer {

struct RknnVisionBackendConfig {
    std::string model_path;
    std::string model_name{"MobileNetV1 RK3576"};
    std::size_t top_k{5};
};

class RknnVisionBackend final : public IVisionBackend {
public:
    RknnVisionBackend(IInferenceScheduler& scheduler, RknnVisionBackendConfig config);
    ~RknnVisionBackend() override;
    RknnVisionBackend(const RknnVisionBackend&) = delete;
    RknnVisionBackend& operator=(const RknnVisionBackend&) = delete;

    protocol::Status load() override;
    protocol::Status infer(const VisionTensor& input, VisionResult& result) override;
    void unload() override;
    ModelState state() const override;
    VisionBackendInfo info() const override;

private:
    IInferenceScheduler& scheduler_;
    const RknnVisionBackendConfig config_;
    mutable std::mutex mutex_;
    ModelState state_{ModelState::Unloaded};
    VisionBackendInfo info_;
    std::uint64_t context_{0};
    std::uint32_t output_elements_{0};
};

}  // namespace cockpit::infer
