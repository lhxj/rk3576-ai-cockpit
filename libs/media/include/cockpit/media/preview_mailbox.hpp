#pragma once

#include "cockpit/media/captured_frame.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace cockpit::media {

struct PreviewDelivery {
    std::uint64_t delivery_id{0};
    std::shared_ptr<const CapturedFrame> frame;
};

class PreviewMailbox {
public:
    void publish(std::shared_ptr<const CapturedFrame> frame);
    bool wait_next(std::uint64_t last_delivery_id, std::chrono::milliseconds timeout,
                   PreviewDelivery& delivery);
    void stop();
    void reset();
    [[nodiscard]] std::uint64_t drop_count() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::shared_ptr<const CapturedFrame> latest_;
    std::uint64_t latest_id_{0};
    std::uint64_t delivered_id_{0};
    std::uint64_t drops_{0};
    bool stopped_{false};
};

class PreviewEpochFilter {
public:
    bool accept(const CapturedFrame& frame);
    [[nodiscard]] std::uint64_t stale_drop_count() const noexcept { return stale_drops_; }
    [[nodiscard]] std::uint64_t current_epoch() const noexcept { return epoch_; }

private:
    std::uint64_t epoch_{0};
    std::uint64_t sequence_{0};
    std::uint64_t stale_drops_{0};
};

}  // namespace cockpit::media
