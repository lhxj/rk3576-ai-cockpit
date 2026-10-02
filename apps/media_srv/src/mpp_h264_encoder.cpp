#include "cockpit/media/mpp_h264_encoder.hpp"
#include "cockpit/media/h264_annex_b.hpp"

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_mpi_cmd.h>
#include <rockchip/rk_venc_cfg.h>
#include <rockchip/rk_venc_cmd.h>
#include <rockchip/rk_venc_rc.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace cockpit::media {
namespace {

std::uint32_t align16(std::uint32_t value) { return (value + 15U) & ~15U; }

std::int64_t steady_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

MediaStatus mpp_error(const char* operation, MPP_RET code) {
    std::ostringstream detail;
    detail << operation << " MPP_RET=" << code;
    return {MediaStatusCode::EncodeError, detail.str()};
}

}  // namespace

struct MppH264Encoder::Impl {
    MediaStatus start(const EncoderConfig& requested, const CameraFormat& input,
                      EncodedPacketCallback packet_callback) {
        std::lock_guard<std::mutex> lock(mutex);
        if (active) return MediaStatus::Ok("MPP encoder already active");
        if (worker.joinable())
            return {MediaStatusCode::InvalidState,
                    "previous MPP encoder worker not joined"};
        if (requested.queue_capacity == 0 || requested.fps_denominator == 0 ||
            input.pixel_format != "NV12" || input.width == 0 || input.height == 0 ||
            input.bytes_per_line < input.width || !packet_callback)
            return {MediaStatusCode::InvalidArgument, "MPP encoder configuration"};
        config = requested;
        format = input;
        callback = std::move(packet_callback);
        hor_stride = align16(std::max(input.width, input.bytes_per_line));
        ver_stride = align16(input.height);
        frame_size = static_cast<std::size_t>(hor_stride) * ver_stride * 3U / 2U;
        const auto prior_starts = stats.start_count;
        stats = {};
        stats.start_count = prior_starts + 1U;
        terminal = MediaStatus::Ok();
        auto status = initialize_mpp();
        if (!status.ok()) {
            finalize_mpp();
            return status;
        }
        queue.clear();
        accepting = true;
        stop_requested = false;
        first_packet_written = false;
        idr_requested = false;
        active = true;
        try {
            worker = std::thread(&Impl::run, this);
        } catch (...) {
            active = false;
            accepting = false;
            finalize_mpp();
            return {MediaStatusCode::Unavailable, "MPP encoder worker start"};
        }
        return MediaStatus::Ok();
    }

    MediaStatus submit(std::shared_ptr<const CapturedFrame> frame) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!active || !accepting) {
            return terminal.ok()
                       ? MediaStatus{MediaStatusCode::InvalidState,
                                     "MPP encoder not accepting"}
                       : terminal;
        }
        if (!frame || frame->pixel_format != "NV12" ||
            frame->width != format.width || frame->height != format.height ||
            frame->bytes_per_line != format.bytes_per_line)
            return {MediaStatusCode::InvalidArgument, "MPP encoder frame format"};
        if (queue.size() >= config.queue_capacity) {
            ++stats.overflow_count;
            fail_locked({MediaStatusCode::RecordingBackpressure,
                         "ENCODER_BACKPRESSURE"});
            ready.notify_all();
            first_packet.notify_all();
            return terminal;
        }
        if (stats.input_frames == 0) stats.first_input_steady_ns = steady_now_ns();
        ++stats.input_frames;
        queue.push_back(std::move(frame));
        stats.queue_peak_depth = std::max(stats.queue_peak_depth, queue.size());
        ready.notify_one();
        return MediaStatus::Ok();
    }

    MediaStatus wait_for_first(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!first_packet.wait_for(lock, timeout, [&] {
                return first_packet_written || !terminal.ok() || !active;
            }))
            return {MediaStatusCode::Timeout, "first MPP packet timeout"};
        return first_packet_written ? MediaStatus::Ok() : terminal;
    }

    MediaStatus stop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active && !worker.joinable()) return terminal;
            accepting = false;
            stop_requested = true;
        }
        ready.notify_all();
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> lock(mutex);
        active = false;
        callback = {};
        first_packet.notify_all();
        return terminal;
    }

    MediaStatus request_idr() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!active) return {MediaStatusCode::InvalidState, "MPP encoder not active"};
        idr_requested = true;
        ++stats.idr_requests;
        return MediaStatus::Ok();
    }

    void run() {
        for (;;) {
            std::shared_ptr<const CapturedFrame> frame;
            bool eos = false;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [&] { return stop_requested || !queue.empty(); });
                if (queue.empty()) {
                    if (stop_requested) break;
                    continue;
                }
                frame = std::move(queue.front());
                queue.pop_front();
                eos = stop_requested && queue.empty();
            }
            auto status = encode(*frame, eos);
            if (!status.ok()) {
                std::lock_guard<std::mutex> lock(mutex);
                ++stats.encoder_errors;
                fail_locked(std::move(status));
                queue.clear();
                first_packet.notify_all();
                break;
            }
            if (eos) break;
        }
        finalize_mpp();
        std::lock_guard<std::mutex> lock(mutex);
        active = false;
        first_packet.notify_all();
    }

    MediaStatus initialize_mpp() {
        MPP_RET ret = mpp_create(&ctx, &mpi);
        if (ret != MPP_OK) return mpp_error("mpp_create", ret);
        ret = mpp_init(ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
        if (ret != MPP_OK) return mpp_error("mpp_init AVC", ret);
        MppEncCfg enc_cfg = nullptr;
        ret = mpp_enc_cfg_init(&enc_cfg);
        if (ret != MPP_OK) return mpp_error("mpp_enc_cfg_init", ret);
        const auto set = [&](const char* key, RK_S32 value) {
            return mpp_enc_cfg_set_s32(enc_cfg, key, value);
        };
        ret = mpi->control(ctx, MPP_ENC_GET_CFG, enc_cfg);
        if (ret == MPP_OK) ret = set("prep:width", static_cast<RK_S32>(format.width));
        if (ret == MPP_OK) ret = set("prep:height", static_cast<RK_S32>(format.height));
        if (ret == MPP_OK) ret = set("prep:hor_stride", static_cast<RK_S32>(hor_stride));
        if (ret == MPP_OK) ret = set("prep:ver_stride", static_cast<RK_S32>(ver_stride));
        if (ret == MPP_OK) ret = set("prep:format", MPP_FMT_YUV420SP);
        if (ret == MPP_OK) ret = set("rc:mode", MPP_ENC_RC_MODE_CBR);
        if (ret == MPP_OK) ret = set("rc:fps_in_flex", 0);
        if (ret == MPP_OK) ret = set("rc:fps_in_num", static_cast<RK_S32>(config.fps_numerator));
        if (ret == MPP_OK) ret = set("rc:fps_in_denom", static_cast<RK_S32>(config.fps_denominator));
        if (ret == MPP_OK) ret = set("rc:fps_out_flex", 0);
        if (ret == MPP_OK) ret = set("rc:fps_out_num", static_cast<RK_S32>(config.fps_numerator));
        if (ret == MPP_OK) ret = set("rc:fps_out_denom", static_cast<RK_S32>(config.fps_denominator));
        if (ret == MPP_OK) ret = set("rc:gop", static_cast<RK_S32>(config.gop));
        if (ret == MPP_OK) ret = set("rc:bps_target", static_cast<RK_S32>(config.bitrate_target));
        if (ret == MPP_OK) ret = set("rc:bps_min", static_cast<RK_S32>(config.bitrate_min));
        if (ret == MPP_OK) ret = set("rc:bps_max", static_cast<RK_S32>(config.bitrate_max));
        if (ret == MPP_OK) ret = set("rc:qp_init", 26);
        if (ret == MPP_OK) ret = set("rc:qp_min", 10);
        if (ret == MPP_OK) ret = set("rc:qp_max", 51);
        if (ret == MPP_OK) ret = set("codec:type", MPP_VIDEO_CodingAVC);
        if (ret == MPP_OK) ret = set("h264:profile", static_cast<RK_S32>(config.h264_profile));
        if (ret == MPP_OK) ret = set("h264:level", static_cast<RK_S32>(config.h264_level));
        if (ret == MPP_OK) ret = set("h264:cabac_en", 1);
        if (ret == MPP_OK) ret = set("h264:cabac_idc", 0);
        if (ret == MPP_OK) ret = mpi->control(ctx, MPP_ENC_SET_CFG, enc_cfg);
        mpp_enc_cfg_deinit(enc_cfg);
        if (ret != MPP_OK) return mpp_error("MPP_ENC_SET_CFG", ret);
        MppEncHeaderMode mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &mode);
        if (ret != MPP_OK) return mpp_error("MPP_ENC_SET_HEADER_MODE", ret);
        RK_S64 timeout_ms = 2000;
        ret = mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout_ms);
        if (ret != MPP_OK) return mpp_error("MPP_SET_OUTPUT_TIMEOUT", ret);
        ret = mpp_buffer_group_get_internal(&input_group, MPP_BUFFER_TYPE_DRM);
        if (ret != MPP_OK) return mpp_error("mpp_buffer_group_get_internal", ret);
        ret = mpp_buffer_get(input_group, &input_buffer, frame_size);
        if (ret != MPP_OK) return mpp_error("mpp_buffer_get", ret);
        return MediaStatus::Ok();
    }

    MediaStatus encode(const CapturedFrame& frame, bool eos) {
        bool force_idr = false;
        EncodedPacketCallback packet_callback;
        {
            std::lock_guard<std::mutex> lock(mutex);
            force_idr = idr_requested;
            idr_requested = false;
            packet_callback = callback;
        }
        if (force_idr) {
            const auto idr_ret = mpi->control(ctx, MPP_ENC_SET_IDR_FRAME, nullptr);
            if (idr_ret != MPP_OK) return mpp_error("MPP_ENC_SET_IDR_FRAME", idr_ret);
        }
        auto* destination = static_cast<std::uint8_t*>(mpp_buffer_get_ptr(input_buffer));
        if (!destination)
            return {MediaStatusCode::EncodeError, "MPP input buffer pointer"};
        const std::size_t source_y_size =
            static_cast<std::size_t>(frame.bytes_per_line) * frame.height;
        const std::size_t source_uv_size = source_y_size / 2U;
        if (frame.payload.size() < source_y_size + source_uv_size)
            return {MediaStatusCode::InvalidArgument, "NV12 payload is truncated"};
        std::memset(destination, 0, frame_size);
        for (std::uint32_t row = 0; row < frame.height; ++row) {
            std::memcpy(destination + static_cast<std::size_t>(row) * hor_stride,
                        frame.payload.data() + static_cast<std::size_t>(row) * frame.bytes_per_line,
                        frame.bytes_per_line);
        }
        auto* destination_uv = destination + static_cast<std::size_t>(hor_stride) * ver_stride;
        std::memset(destination_uv, 128,
                    static_cast<std::size_t>(hor_stride) * ver_stride / 2U);
        const auto* source_uv = frame.payload.data() + source_y_size;
        for (std::uint32_t row = 0; row < frame.height / 2U; ++row) {
            std::memcpy(destination_uv + static_cast<std::size_t>(row) * hor_stride,
                        source_uv + static_cast<std::size_t>(row) * frame.bytes_per_line,
                        frame.bytes_per_line);
        }
        MPP_RET ret = mpp_buffer_sync_end(input_buffer);
        if (ret != MPP_OK) return mpp_error("mpp_buffer_sync_end", ret);
        MppFrame mpp_frame = nullptr;
        ret = mpp_frame_init(&mpp_frame);
        if (ret != MPP_OK) return mpp_error("mpp_frame_init", ret);
        mpp_frame_set_width(mpp_frame, format.width);
        mpp_frame_set_height(mpp_frame, format.height);
        mpp_frame_set_hor_stride(mpp_frame, hor_stride);
        mpp_frame_set_ver_stride(mpp_frame, ver_stride);
        mpp_frame_set_fmt(mpp_frame, MPP_FMT_YUV420SP);
        mpp_frame_set_buffer(mpp_frame, input_buffer);
        mpp_frame_set_pts(mpp_frame, static_cast<RK_S64>(frame.sequence));
        mpp_frame_set_eos(mpp_frame, eos ? 1U : 0U);
        ret = mpi->encode_put_frame(ctx, mpp_frame);
        mpp_frame_deinit(&mpp_frame);
        if (ret != MPP_OK) return mpp_error("encode_put_frame", ret);
        bool more = false;
        std::vector<std::uint8_t> access_unit;
        std::uint64_t packet_count = 0;
        do {
            MppPacket packet = nullptr;
            ret = mpi->encode_get_packet(ctx, &packet);
            if (ret != MPP_OK || !packet)
                return ret == MPP_OK
                           ? MediaStatus{MediaStatusCode::EncodeError,
                                         "encode_get_packet returned no packet"}
                           : mpp_error("encode_get_packet", ret);
            const auto length = mpp_packet_get_length(packet);
            const auto* data = static_cast<const std::uint8_t*>(mpp_packet_get_pos(packet));
            if (length > 0 && data)
                access_unit.insert(access_unit.end(), data, data + length);
            const bool partition = mpp_packet_is_partition(packet) != 0;
            const bool eoi = mpp_packet_is_eoi(packet) != 0;
            mpp_packet_deinit(&packet);
            ++packet_count;
            more = partition && !eoi;
        } while (more);
        if (access_unit.empty())
            return {MediaStatusCode::EncodeError, "MPP produced empty access unit"};
        auto encoded = std::make_shared<EncodedPacket>();
        encoded->annex_b = std::move(access_unit);
        encoded->camera_sequence = frame.sequence;
        encoded->stream_epoch = frame.stream_epoch;
        encoded->rtp_timestamp = static_cast<std::uint32_t>(
            frame.sequence * 90000ULL * config.fps_denominator / config.fps_numerator);
        encoded->encoded_steady_ns = steady_now_ns();
        encoded->key_frame = contains_h264_nal_type(encoded->annex_b, 5U);
        if (packet_callback) packet_callback(encoded);
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++stats.encoded_frames;
            stats.packets += packet_count;
            stats.output_bytes += encoded->annex_b.size();
            stats.last_output_steady_ns = encoded->encoded_steady_ns;
            first_packet_written = true;
            first_packet.notify_all();
        }
        return MediaStatus::Ok();
    }

    void finalize_mpp() {
        if (input_buffer) {
            mpp_buffer_put(input_buffer);
            input_buffer = nullptr;
        }
        if (input_group) {
            mpp_buffer_group_put(input_group);
            input_group = nullptr;
        }
        if (ctx) {
            (void)mpp_destroy(ctx);
            ctx = nullptr;
            mpi = nullptr;
        }
    }

    void fail_locked(MediaStatus status) {
        if (terminal.ok()) terminal = std::move(status);
        accepting = false;
        stop_requested = true;
        stats.last_error = terminal.detail;
    }

    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable first_packet;
    EncoderConfig config;
    CameraFormat format;
    std::deque<std::shared_ptr<const CapturedFrame>> queue;
    std::thread worker;
    EncoderStats stats;
    MediaStatus terminal;
    EncodedPacketCallback callback;
    MppCtx ctx{nullptr};
    MppApi* mpi{nullptr};
    MppBufferGroup input_group{nullptr};
    MppBuffer input_buffer{nullptr};
    std::uint32_t hor_stride{0};
    std::uint32_t ver_stride{0};
    std::size_t frame_size{0};
    bool active{false};
    bool accepting{false};
    bool stop_requested{false};
    bool first_packet_written{false};
    bool idr_requested{false};
};

MppH264Encoder::MppH264Encoder() : impl_(std::make_unique<Impl>()) {}
MppH264Encoder::~MppH264Encoder() { (void)impl_->stop(); }
MediaStatus MppH264Encoder::start(const EncoderConfig& config, const CameraFormat& format,
                                  EncodedPacketCallback callback) {
    return impl_->start(config, format, std::move(callback));
}
MediaStatus MppH264Encoder::submit(std::shared_ptr<const CapturedFrame> frame) {
    return impl_->submit(std::move(frame));
}
MediaStatus MppH264Encoder::wait_for_first_packet(std::chrono::milliseconds timeout) {
    return impl_->wait_for_first(timeout);
}
MediaStatus MppH264Encoder::request_idr() { return impl_->request_idr(); }
MediaStatus MppH264Encoder::stop() { return impl_->stop(); }
bool MppH264Encoder::active() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->active;
}
EncoderStats MppH264Encoder::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

}  // namespace cockpit::media
