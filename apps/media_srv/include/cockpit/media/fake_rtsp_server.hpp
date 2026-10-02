#pragma once

#include "cockpit/media/rtsp_server.hpp"

#include <memory>
#include <mutex>

namespace cockpit::media {

struct FakeRtspServerOptions {
    bool fail_start{false};
    bool fail_submit{false};
};

class FakeRtspServer final : public IRtspServer {
public:
    explicit FakeRtspServer(FakeRtspServerOptions options = {});
    MediaStatus start(const RtspConfig& config, const CameraFormat& format,
                      RequestIdrCallback request_idr) override;
    MediaStatus submit(std::shared_ptr<const EncodedPacket> packet) override;
    MediaStatus wait_for_parameters(std::chrono::milliseconds timeout) override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] RtspStats stats() const override;
    [[nodiscard]] std::size_t start_count() const;
    [[nodiscard]] std::size_t stop_count() const;
    [[nodiscard]] CameraFormat last_format() const;

private:
    const FakeRtspServerOptions options_;
    mutable std::mutex mutex_;
    RtspStats stats_;
    CameraFormat last_format_;
    std::size_t start_count_{0};
    std::size_t stop_count_{0};
    bool active_{false};
};

}  // namespace cockpit::media
