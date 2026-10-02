#pragma once

#include "cockpit/media/h264_encoder.hpp"

#include <memory>

namespace cockpit::media {

class MppH264Encoder final : public IH264Encoder {
public:
    MppH264Encoder();
    ~MppH264Encoder() override;
    MediaStatus start(const EncoderConfig& config, const CameraFormat& format,
                      EncodedPacketCallback callback) override;
    MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) override;
    MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) override;
    MediaStatus request_idr() override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] EncoderStats stats() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpit::media
