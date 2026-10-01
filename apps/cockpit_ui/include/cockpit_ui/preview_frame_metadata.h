#pragma once

#include <cstdint>
#include <string>

namespace cockpit::ui {

// Metadata only. Pixel ownership and DMA-BUF transport remain a media_srv
// integration decision. stream_epoch prevents stale frames from a previous
// camera session being displayed after a switch or reconnect.
struct PreviewFrameMetadata {
    std::string camera_id;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string format;
    std::uint64_t sequence{0};
    std::uint64_t stream_epoch{0};
    std::int64_t timestamp_ns{0};

    [[nodiscard]] bool valid() const noexcept {
        return !camera_id.empty() && width > 0 && height > 0 && !format.empty() &&
               stream_epoch > 0;
    }
};

}  // namespace cockpit::ui
