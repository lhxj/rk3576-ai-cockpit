#include "cockpit/media/fake_camera_capture.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

namespace cockpit::media {
namespace {

std::int64_t steady_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

FakeCameraCapture::FakeCameraCapture(FakeCameraCaptureOptions options)
    : options_(std::move(options)), start_released_(!options_.hold_start) {}

FakeCameraCapture::~FakeCameraCapture() { close_device(); }

MediaStatus FakeCameraCapture::open_device(const std::string& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) return MediaStatus::Ok("already open");
    if (device.empty()) return {MediaStatusCode::InvalidArgument, "empty device"};
    if (options_.fail_open) return {MediaStatusCode::Unavailable, "fake open failure"};
    opened_ = true;
    return MediaStatus::Ok();
}

MediaStatus FakeCameraCapture::configure(const CameraCaptureConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) return {MediaStatusCode::InvalidState, "fake device is closed"};
    if (options_.fail_configure)
        return {MediaStatusCode::UnsupportedCameraFormat, "fake configure failure"};
    if (config.width == 0 || config.height == 0 || config.pixel_format != "NV12" ||
        (config.width % 2) != 0 || (config.height % 2) != 0)
        return {MediaStatusCode::UnsupportedCameraFormat, "fake supports even NV12 only"};
    config_ = config;
    const auto size = config.width * config.height * 3U / 2U;
    actual_ = {config.camera_id, config.width, config.height, config.pixel_format, 1,
               config.width, size, config.buffer_count, config.buffer_count,
               1, config.fps, true, "fake"};
    source_.assign(size, 0);
    configured_ = true;
    return MediaStatus::Ok();
}

MediaStatus FakeCameraCapture::start(FrameCallback callback) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (streaming_) return MediaStatus::Ok("already streaming");
    if (!configured_) return {MediaStatusCode::InvalidState, "fake capture not configured"};
    start_entered_ = true;
    start_gate_.notify_all();
    start_gate_.wait(lock, [&] { return start_released_; });
    if (options_.fail_start) return {MediaStatusCode::IoError, "fake stream start failure"};
    if (!callback) return {MediaStatusCode::InvalidArgument, "frame callback"};
    callback_ = std::move(callback);
    stop_requested_ = false;
    streaming_ = true;
    stats_ = {};
    stats_.bytes_used_min = std::numeric_limits<std::uint32_t>::max();
    stats_.stream_epoch = ++next_epoch_;
    worker_ = std::thread(&FakeCameraCapture::capture_loop, this);
    return MediaStatus::Ok();
}

MediaStatus FakeCameraCapture::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!streaming_ && !worker_.joinable()) return MediaStatus::Ok("already stopped");
        stop_requested_ = true;
    }
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    streaming_ = false;
    callback_ = {};
    return MediaStatus::Ok();
}

void FakeCameraCapture::close_device() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    opened_ = false;
    configured_ = false;
    source_.clear();
}

CameraFormat FakeCameraCapture::actual_format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return actual_;
}

CaptureStats FakeCameraCapture::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool FakeCameraCapture::streaming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streaming_;
}

void FakeCameraCapture::release_start() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        start_released_ = true;
    }
    start_gate_.notify_all();
}

bool FakeCameraCapture::wait_until_start_entered(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return start_gate_.wait_for(lock, timeout, [&] { return start_entered_; });
}

std::vector<std::uint8_t> FakeCameraCapture::last_source_buffer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return source_;
}

void FakeCameraCapture::capture_loop() {
    std::uint64_t sequence = 0;
    for (;;) {
        FrameCallback callback;
        CapturedFrame frame;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_requested_) break;
            const auto y_size = static_cast<std::size_t>(actual_.bytes_per_line) * actual_.height;
            std::fill(source_.begin(), source_.begin() + static_cast<std::ptrdiff_t>(y_size),
                      static_cast<std::uint8_t>(32U + (sequence % 160U)));
            std::fill(source_.begin() + static_cast<std::ptrdiff_t>(y_size), source_.end(), 128U);
            const auto now = steady_now_ns();
            frame = {actual_.camera_id, actual_.width, actual_.height, actual_.pixel_format,
                     actual_.bytes_per_line, actual_.size_image, actual_.size_image,
                     sequence, stats_.stream_epoch, now, now, TimestampClock::Monotonic,
                     source_};
            callback = callback_;
            ++stats_.frames;
            stats_.bytes_used_min = std::min(stats_.bytes_used_min, actual_.size_image);
            stats_.bytes_used_max = std::max(stats_.bytes_used_max, actual_.size_image);
            if (stats_.first_dequeue_steady_ns == 0) stats_.first_dequeue_steady_ns = now;
            stats_.last_dequeue_steady_ns = now;
        }
        if (callback) callback(std::move(frame));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::fill(source_.begin(), source_.end(), 0xEEU);
        }
        ++sequence;
        std::this_thread::sleep_for(options_.frame_interval);
    }
}

}  // namespace cockpit::media
