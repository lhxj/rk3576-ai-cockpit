#include "cockpit/media/fake_h264_encoder.hpp"

#include <algorithm>

namespace cockpit::media {
namespace {
std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::vector<std::uint8_t> fake_au(bool idr) {
    std::vector<std::uint8_t> bytes;
    auto add = [&](std::initializer_list<std::uint8_t> nal) {
        bytes.insert(bytes.end(), {0, 0, 0, 1});
        bytes.insert(bytes.end(), nal);
    };
    if (idr) {
        add({0x67, 0x64, 0x00, 0x28, 0xAC, 0x2B, 0x40});
        add({0x68, 0xEE, 0x3C, 0x80});
        add({0x65, 0x88, 0x84, 0x21, 0xA0});
    } else {
        add({0x41, 0x9A, 0x22, 0x11});
    }
    return bytes;
}
}

FakeH264Encoder::FakeH264Encoder(FakeH264EncoderOptions options)
    : options_(options) {}
FakeH264Encoder::~FakeH264Encoder() { (void)stop(); }

MediaStatus FakeH264Encoder::start(const EncoderConfig& config,
                                   const CameraFormat& format,
                                   EncodedPacketCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) return MediaStatus::Ok("fake encoder already active");
    if (options_.fail_start)
        return {MediaStatusCode::Unavailable, "fake encoder start failure"};
    if (!callback || config.queue_capacity == 0 || config.fps_denominator == 0)
        return {MediaStatusCode::InvalidArgument, "fake encoder configuration"};
    config_ = config;
    format_ = format;
    callback_ = std::move(callback);
    const auto prior_starts = stats_.start_count;
    stats_ = {};
    stats_.start_count = prior_starts + 1U;
    terminal_ = MediaStatus::Ok();
    queue_.clear();
    accepting_ = true;
    stopping_ = false;
    first_encoded_ = false;
    force_idr_ = true;
    active_ = true;
    try { worker_ = std::thread(&FakeH264Encoder::run, this); }
    catch (...) {
        active_ = false;
        accepting_ = false;
        return {MediaStatusCode::Unavailable, "fake encoder worker start"};
    }
    return MediaStatus::Ok();
}

MediaStatus FakeH264Encoder::submit(std::shared_ptr<const CapturedFrame> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !accepting_)
        return terminal_.ok() ? MediaStatus{MediaStatusCode::InvalidState,
                                            "fake encoder not accepting"} : terminal_;
    if (!frame) return {MediaStatusCode::InvalidArgument, "fake frame"};
    if (queue_.size() >= config_.queue_capacity) {
        ++stats_.overflow_count;
        terminal_ = {MediaStatusCode::RecordingBackpressure,
                     "ENCODER_BACKPRESSURE"};
        stats_.last_error = terminal_.detail;
        accepting_ = false;
        stopping_ = true;
        ready_.notify_all();
        first_packet_.notify_all();
        return terminal_;
    }
    if (stats_.input_frames == 0) stats_.first_input_steady_ns = now_ns();
    ++stats_.input_frames;
    queue_.push_back(std::move(frame));
    stats_.queue_peak_depth = std::max(stats_.queue_peak_depth, queue_.size());
    ready_.notify_one();
    return MediaStatus::Ok();
}

MediaStatus FakeH264Encoder::wait_for_first_packet(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!first_packet_.wait_for(lock, timeout, [&] {
            return first_encoded_ || !terminal_.ok() || !active_;
        })) return {MediaStatusCode::Timeout, "first fake packet timeout"};
    return first_encoded_ ? MediaStatus::Ok() : terminal_;
}

MediaStatus FakeH264Encoder::request_idr() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_) return {MediaStatusCode::InvalidState, "encoder not active"};
    force_idr_ = true;
    ++stats_.idr_requests;
    return MediaStatus::Ok();
}

MediaStatus FakeH264Encoder::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ && !worker_.joinable()) return terminal_;
        accepting_ = false;
        stopping_ = true;
    }
    ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = false;
    callback_ = {};
    first_packet_.notify_all();
    return terminal_;
}

bool FakeH264Encoder::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}
EncoderStats FakeH264Encoder::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void FakeH264Encoder::run() {
    for (;;) {
        std::shared_ptr<const CapturedFrame> frame;
        bool idr = false;
        EncodedPacketCallback callback;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) { if (stopping_) break; continue; }
            frame = std::move(queue_.front());
            queue_.pop_front();
            idr = force_idr_ || stats_.encoded_frames == 0 ||
                  (config_.gop != 0 && stats_.encoded_frames % config_.gop == 0);
            force_idr_ = false;
            callback = callback_;
        }
        if (options_.encode_delay.count() > 0)
            std::this_thread::sleep_for(options_.encode_delay);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (options_.fail_after_packets != 0 &&
                stats_.encoded_frames >= options_.fail_after_packets) {
                ++stats_.encoder_errors;
                terminal_ = {MediaStatusCode::EncodeError,
                             "fake asynchronous encode failure"};
                stats_.last_error = terminal_.detail;
                accepting_ = false;
                stopping_ = true;
                queue_.clear();
                first_packet_.notify_all();
                continue;
            }
        }
        auto packet = std::make_shared<EncodedPacket>();
        packet->annex_b = fake_au(idr);
        packet->camera_sequence = frame->sequence;
        packet->stream_epoch = frame->stream_epoch;
        packet->rtp_timestamp = static_cast<std::uint32_t>(
            frame->sequence * 90000ULL * config_.fps_denominator /
            config_.fps_numerator);
        packet->encoded_steady_ns = now_ns();
        packet->key_frame = idr;
        if (callback) callback(packet);
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.encoded_frames;
        ++stats_.packets;
        stats_.output_bytes += packet->annex_b.size();
        stats_.last_output_steady_ns = packet->encoded_steady_ns;
        first_encoded_ = true;
        first_packet_.notify_all();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = false;
    first_packet_.notify_all();
}

}  // namespace cockpit::media
