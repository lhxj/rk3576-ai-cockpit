#pragma once

#include "cockpit/media/media_recorder.hpp"

#include <memory>

namespace cockpit::media {

class MppH264Recorder final : public IMediaRecorder {
public:
    MppH264Recorder();
    ~MppH264Recorder() override;

    MediaStatus start(const RecorderConfig& config, const CameraFormat& format,
                      const std::string& output_path) override;
    MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) override;
    MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] RecorderStats stats() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpit::media
