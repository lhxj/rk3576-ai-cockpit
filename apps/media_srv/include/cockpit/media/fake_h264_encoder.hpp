#pragma once

#include "cockpit/media/h264_encoder.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace cockpit::media {

struct FakeH264EncoderOptions {
    bool fail_start{false};
    std::size_t fail_after_packets{0};
    std::chrono::milliseconds encode_delay{0};
};

class FakeH264Encoder final : public IH264Encoder {
public:
    explicit FakeH264Encoder(FakeH264EncoderOptions options = {});
    ~FakeH264Encoder() override;
    MediaStatus start(const EncoderConfig& config, const CameraFormat& format,
                      EncodedPacketCallback callback) override;
    MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) override;
    MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) override;
    MediaStatus request_idr() override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] EncoderStats stats() const override;

private:
    void run();
    const FakeH264EncoderOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable first_packet_;
    std::deque<std::shared_ptr<const CapturedFrame>> queue_;
    std::thread worker_;
    EncoderConfig config_;
    CameraFormat format_;
    EncodedPacketCallback callback_;
    EncoderStats stats_;
    MediaStatus terminal_;
    bool active_{false};
    bool accepting_{false};
    bool stopping_{false};
    bool first_encoded_{false};
    bool force_idr_{false};
};

}  // namespace cockpit::media
