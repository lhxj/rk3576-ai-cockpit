#pragma once

#include "cockpit/media/preview_mailbox.hpp"
#include "cockpit_ui/preview_frame_metadata.h"

#include <QImage>
#include <QObject>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace cockpit::ui {

struct QtPreviewStats {
    std::uint64_t converted_frames{0};
    std::uint64_t delivered_frames{0};
    std::uint64_t throttled_frames{0};
    std::uint64_t ui_drops{0};
    std::uint64_t mailbox_drops{0};
    double displayed_fps{0.0};
};

class QtPreviewBridge final : public QObject {
public:
    using FrameCallback = std::function<void(QImage, PreviewFrameMetadata, double,
                                             std::uint64_t, std::uint64_t)>;

    explicit QtPreviewBridge(std::shared_ptr<media::PreviewMailbox> mailbox);
    ~QtPreviewBridge() override;

    bool start(FrameCallback callback);
    void stop();
    [[nodiscard]] QtPreviewStats stats() const;

private:
    struct PendingImage {
        QImage image;
        PreviewFrameMetadata metadata;
        double preview_fps{0.0};
        std::uint64_t mailbox_drops{0};
        std::uint64_t ui_drops{0};
    };

    void run();
    void deliver_latest();

    std::shared_ptr<media::PreviewMailbox> mailbox_;
    FrameCallback callback_;
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> delivery_queued_{false};
    mutable std::mutex mutex_;
    PendingImage pending_;
    std::uint64_t pending_generation_{0};
    std::uint64_t delivered_generation_{0};
    std::uint64_t ui_drops_{0};
    std::uint64_t converted_frames_{0};
    std::uint64_t delivered_frames_{0};
    std::uint64_t throttled_frames_{0};
    double displayed_fps_{0.0};
    std::chrono::steady_clock::time_point first_delivery_{};
};

}  // namespace cockpit::ui
