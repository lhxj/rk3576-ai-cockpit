#include "cockpit/media/fake_media_recorder.hpp"

#include <array>
#include <chrono>
#include <utility>

namespace cockpit::media {
namespace {

std::int64_t steady_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

FakeMediaRecorder::FakeMediaRecorder(FakeMediaRecorderOptions options)
    : options_(std::move(options)) {}

FakeMediaRecorder::~FakeMediaRecorder() { (void)stop(); }

MediaStatus FakeMediaRecorder::start(const RecorderConfig& config, const CameraFormat& format,
                                     const std::string& output_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) return MediaStatus::Ok("fake recorder already active");
    if (worker_.joinable())
        return {MediaStatusCode::InvalidState, "previous fake recorder worker not joined"};
    if (options_.fail_start) return {MediaStatusCode::Unavailable, "fake recorder init failure"};
    if (config.queue_capacity == 0 || format.pixel_format != "NV12" || output_path.empty())
        return {MediaStatusCode::InvalidArgument, "fake recorder configuration"};
    output_.open(output_path, std::ios::binary | std::ios::trunc);
    if (!output_) return {MediaStatusCode::IoError, "open recording output"};
    config_ = config;
    format_ = format;
    queue_.clear();
    stats_ = {};
    stats_.file_closed = false;
    stats_.output_path = output_path;
    terminal_status_ = MediaStatus::Ok();
    active_ = true;
    accepting_ = true;
    stop_requested_ = false;
    first_packet_written_ = false;
    try {
        worker_ = std::thread(&FakeMediaRecorder::run, this);
    } catch (...) {
        output_.close();
        stats_.file_closed = true;
        active_ = false;
        accepting_ = false;
        return {MediaStatusCode::Unavailable, "fake recorder worker start"};
    }
    return MediaStatus::Ok();
}

MediaStatus FakeMediaRecorder::submit(std::shared_ptr<const CapturedFrame> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !accepting_) {
        return terminal_status_.ok()
                   ? MediaStatus{MediaStatusCode::InvalidState, "recorder not accepting"}
                   : terminal_status_;
    }
    if (!frame) return {MediaStatusCode::InvalidArgument, "recording frame"};
    if (queue_.size() >= config_.queue_capacity) {
        ++stats_.overflow_count;
        fail_locked({MediaStatusCode::RecordingBackpressure, "RECORDING_BACKPRESSURE"});
        ready_.notify_all();
        first_packet_.notify_all();
        return terminal_status_;
    }
    if (stats_.input_frames == 0) stats_.first_input_steady_ns = steady_now_ns();
    ++stats_.input_frames;
    queue_.push_back(std::move(frame));
    stats_.queue_peak_depth = std::max(stats_.queue_peak_depth, queue_.size());
    ready_.notify_one();
    return MediaStatus::Ok();
}

MediaStatus FakeMediaRecorder::wait_for_first_packet(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!first_packet_.wait_for(lock, timeout, [&] {
            return first_packet_written_ || !terminal_status_.ok() || !active_;
        }))
        return {MediaStatusCode::Timeout, "first encoded packet timeout"};
    return first_packet_written_ ? MediaStatus::Ok() : terminal_status_;
}

MediaStatus FakeMediaRecorder::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ && !worker_.joinable()) return terminal_status_;
        accepting_ = false;
        stop_requested_ = true;
    }
    ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    if (output_.is_open()) output_.close();
    stats_.file_closed = true;
    active_ = false;
    first_packet_.notify_all();
    return terminal_status_;
}

bool FakeMediaRecorder::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

RecorderStats FakeMediaRecorder::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void FakeMediaRecorder::run() {
    for (;;) {
        std::shared_ptr<const CapturedFrame> frame;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&] { return stop_requested_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stop_requested_) break;
                continue;
            }
            frame = std::move(queue_.front());
            queue_.pop_front();
        }
        if (options_.encode_delay.count() > 0) std::this_thread::sleep_for(options_.encode_delay);
        std::lock_guard<std::mutex> lock(mutex_);
        if (options_.fail_encode ||
            (options_.fail_after_packets > 0 &&
             stats_.packets >= options_.fail_after_packets)) {
            ++stats_.encoder_errors;
            fail_locked({MediaStatusCode::EncodeError, "fake encode failure"});
            queue_.clear();
            first_packet_.notify_all();
            break;
        }
        const std::array<unsigned char, 8> packet = {
            0x00, 0x00, 0x00, 0x01,
            static_cast<unsigned char>(stats_.encoded_frames == 0 ? 0x65 : 0x41),
            static_cast<unsigned char>(frame->sequence & 0xFFU), 0x80, 0x00};
        output_.write(reinterpret_cast<const char*>(packet.data()),
                      static_cast<std::streamsize>(packet.size()));
        if (!output_) {
            ++stats_.encoder_errors;
            fail_locked({MediaStatusCode::IoError, "fake recording write failure"});
            queue_.clear();
            first_packet_.notify_all();
            break;
        }
        ++stats_.encoded_frames;
        ++stats_.packets;
        stats_.output_bytes += packet.size();
        stats_.last_output_steady_ns = steady_now_ns();
        first_packet_written_ = true;
        first_packet_.notify_all();
    }
}

void FakeMediaRecorder::fail_locked(MediaStatus status) {
    if (terminal_status_.ok()) terminal_status_ = std::move(status);
    accepting_ = false;
    stop_requested_ = true;
    stats_.last_error = terminal_status_.detail;
}

}  // namespace cockpit::media
