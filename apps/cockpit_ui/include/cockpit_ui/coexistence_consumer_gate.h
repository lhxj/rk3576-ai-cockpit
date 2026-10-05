#pragma once
#include <ostream>
#include <stdexcept>
#include <string>

namespace cockpit::ui {
inline std::string bounded_error_hex(const std::string& value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    const auto count = value.size() < 256 ? value.size() : 256;
    result.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(value[i]);
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}

// Test orchestration diagnostic only. The same four active consumers remain mandatory.
template <typename Service>
void require_coexistence_consumers(Service& service, std::ostream& evidence) {
    const bool preview = service.preview_active();
    const bool recording = service.recording_active();
    const bool rtsp = service.rtsp_active();
    const bool vision = service.vision_active();
    if (preview && recording && rtsp && vision) return;
    const auto capture = service.capture_stats();
    const auto recorder = service.recorder_stats();
    const auto encoder = service.encoder_stats();
    const auto stream = service.rtsp_stats();
    const auto operations = service.service_stats();
    evidence << "COEXISTENCE_CONSUMER_STOP_SNAPSHOT preview=" << preview
             << " recording=" << recording << " rtsp=" << rtsp << " vision=" << vision
             << " capture_frames=" << capture.frames
             << " dqbuf_error=" << capture.dequeue_errors << " qbuf_error=" << capture.queue_errors
             << " record_packets=" << recorder.packets << " record_overflow=" << recorder.overflow_count
             << " record_error_hex=" << bounded_error_hex(recorder.last_error)
             << " encoder_frames=" << encoder.encoded_frames << " encoder_error=" << encoder.encoder_errors
             << " encoder_overflow=" << encoder.overflow_count
             << " encoder_error_hex=" << bounded_error_hex(encoder.last_error)
             << " rtp_packets=" << stream.rtp_packet_count << " rtp_drop=" << stream.rtp_drop_count
             << " rtsp_error_hex=" << bounded_error_hex(stream.last_error)
             << " preview_stop_requests=" << operations.preview_stop_requests
             << " recording_stop_requests=" << operations.recording_stop_requests
             << " rtsp_stop_requests=" << operations.rtsp_stop_requests
             << " vision_stop_requests=" << operations.vision_stop_requests
             << " operation_failures=" << operations.operation_failures << '\n';
    throw std::runtime_error("coexistence consumer stopped");
}
} // namespace cockpit::ui
