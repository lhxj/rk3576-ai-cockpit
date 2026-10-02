#include "cockpit/media/file_recording_sink.hpp"

#include <algorithm>

namespace cockpit::media {
namespace {
std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

FileRecordingSink::~FileRecordingSink() { (void)stop(); }

MediaStatus FileRecordingSink::start(const std::string& path, std::size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) return MediaStatus::Ok("file sink already active");
    if (worker_.joinable() || path.empty() || capacity == 0)
        return {MediaStatusCode::InvalidArgument, "file sink configuration"};
    output_.open(path, std::ios::binary | std::ios::trunc);
    if (!output_) return {MediaStatusCode::IoError, "open H.264 output"};
    stats_ = {};
    stats_.file_closed = false;
    stats_.output_path = path;
    terminal_ = MediaStatus::Ok();
    queue_capacity_ = capacity;
    queue_.clear();
    accepting_ = true;
    stopping_ = false;
    first_written_ = false;
    active_ = true;
    try { worker_ = std::thread(&FileRecordingSink::run, this); }
    catch (...) {
        output_.close();
        stats_.file_closed = true;
        active_ = false;
        accepting_ = false;
        return {MediaStatusCode::Unavailable, "file sink worker start"};
    }
    return MediaStatus::Ok();
}

MediaStatus FileRecordingSink::submit(std::shared_ptr<const EncodedPacket> packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !accepting_)
        return terminal_.ok() ? MediaStatus{MediaStatusCode::InvalidState,
                                            "file sink not accepting"} : terminal_;
    if (!packet || packet->annex_b.empty())
        return {MediaStatusCode::InvalidArgument, "empty encoded packet"};
    if (queue_.size() >= queue_capacity_) {
        ++stats_.overflow_count;
        terminal_ = {MediaStatusCode::RecordingBackpressure,
                     "RECORDING_PACKET_BACKPRESSURE"};
        stats_.last_error = terminal_.detail;
        accepting_ = false;
        stopping_ = true;
        ready_.notify_all();
        first_packet_.notify_all();
        return terminal_;
    }
    if (stats_.input_frames == 0) stats_.first_input_steady_ns = now_ns();
    ++stats_.input_frames;
    queue_.push_back(std::move(packet));
    stats_.queue_peak_depth = std::max(stats_.queue_peak_depth, queue_.size());
    ready_.notify_one();
    return MediaStatus::Ok();
}

MediaStatus FileRecordingSink::wait_for_first_packet(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!first_packet_.wait_for(lock, timeout, [&] {
            return first_written_ || !terminal_.ok() || !active_;
        })) return {MediaStatusCode::Timeout, "first file packet timeout"};
    return first_written_ ? MediaStatus::Ok() : terminal_;
}

MediaStatus FileRecordingSink::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ && !worker_.joinable()) return terminal_;
        accepting_ = false;
        stopping_ = true;
    }
    ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    if (output_.is_open()) { output_.flush(); output_.close(); }
    stats_.file_closed = true;
    active_ = false;
    first_packet_.notify_all();
    return terminal_;
}

bool FileRecordingSink::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}
RecorderStats FileRecordingSink::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void FileRecordingSink::run() {
    for (;;) {
        std::shared_ptr<const EncodedPacket> packet;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) { if (stopping_) break; continue; }
            packet = std::move(queue_.front());
            queue_.pop_front();
        }
        output_.write(reinterpret_cast<const char*>(packet->annex_b.data()),
                      static_cast<std::streamsize>(packet->annex_b.size()));
        std::lock_guard<std::mutex> lock(mutex_);
        if (!output_) {
            terminal_ = {MediaStatusCode::IoError, "write H.264 output"};
            stats_.last_error = terminal_.detail;
            accepting_ = false;
            stopping_ = true;
            queue_.clear();
            first_packet_.notify_all();
            continue;
        }
        ++stats_.encoded_frames;
        ++stats_.packets;
        stats_.output_bytes += packet->annex_b.size();
        stats_.last_output_steady_ns = now_ns();
        first_written_ = true;
        first_packet_.notify_all();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (output_.is_open()) { output_.flush(); output_.close(); }
    stats_.file_closed = true;
    active_ = false;
    first_packet_.notify_all();
}

}  // namespace cockpit::media
