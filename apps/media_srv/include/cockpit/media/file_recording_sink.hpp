#pragma once

#include "cockpit/media/h264_encoder.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cockpit::media {

struct RecorderStats {
    std::uint64_t input_frames{0};
    std::uint64_t encoded_frames{0};
    std::uint64_t packets{0};
    std::uint64_t output_bytes{0};
    std::size_t queue_peak_depth{0};
    std::uint64_t overflow_count{0};
    std::uint64_t encoder_errors{0};
    std::int64_t first_input_steady_ns{0};
    std::int64_t last_output_steady_ns{0};
    bool file_closed{true};
    std::string output_path;
    std::string last_error;
};

class IFileRecordingSink {
public:
    virtual ~IFileRecordingSink() = default;
    virtual MediaStatus start(const std::string& output_path,
                              std::size_t queue_capacity) = 0;
    virtual MediaStatus submit(std::shared_ptr<const EncodedPacket> packet) = 0;
    virtual MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) = 0;
    virtual MediaStatus stop() = 0;
    [[nodiscard]] virtual bool active() const = 0;
    [[nodiscard]] virtual RecorderStats stats() const = 0;
};

class FileRecordingSink final : public IFileRecordingSink {
public:
    FileRecordingSink() = default;
    ~FileRecordingSink() override;
    MediaStatus start(const std::string& output_path, std::size_t queue_capacity) override;
    MediaStatus submit(std::shared_ptr<const EncodedPacket> packet) override;
    MediaStatus wait_for_first_packet(std::chrono::milliseconds timeout) override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] RecorderStats stats() const override;

private:
    void run();
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable first_packet_;
    std::deque<std::shared_ptr<const EncodedPacket>> queue_;
    std::ofstream output_;
    std::thread worker_;
    RecorderStats stats_;
    MediaStatus terminal_;
    std::size_t queue_capacity_{0};
    bool active_{false};
    bool accepting_{false};
    bool stopping_{false};
    bool first_written_{false};
};

}  // namespace cockpit::media
