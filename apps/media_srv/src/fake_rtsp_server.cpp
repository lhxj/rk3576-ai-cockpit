#include "cockpit/media/fake_rtsp_server.hpp"

namespace cockpit::media {

FakeRtspServer::FakeRtspServer(FakeRtspServerOptions options) : options_(options) {}

MediaStatus FakeRtspServer::start(const RtspConfig& config, const CameraFormat& format,
                                  RequestIdrCallback request_idr) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) return MediaStatus::Ok("fake RTSP already active");
    if (options_.fail_start)
        return {MediaStatusCode::Unavailable, "fake RTSP start failure"};
    active_ = true;
    ++start_count_;
    stats_ = {};
    last_format_ = format;
    stats_.listen_port = config.port == 0 ? 18554 : config.port;
    if (request_idr) request_idr();
    return MediaStatus::Ok();
}

MediaStatus FakeRtspServer::submit(std::shared_ptr<const EncodedPacket> packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_) return {MediaStatusCode::InvalidState, "fake RTSP stopped"};
    if (options_.fail_submit)
        return {MediaStatusCode::Unavailable, "fake RTSP submit failure"};
    if (!packet) return {MediaStatusCode::InvalidArgument, "fake RTSP packet"};
    ++stats_.rtp_packet_count;
    return MediaStatus::Ok();
}
MediaStatus FakeRtspServer::wait_for_parameters(std::chrono::milliseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ ? MediaStatus::Ok()
                   : MediaStatus{MediaStatusCode::InvalidState, "fake RTSP stopped"};
}

MediaStatus FakeRtspServer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_) ++stop_count_;
    active_ = false;
    return MediaStatus::Ok();
}
bool FakeRtspServer::active() const { std::lock_guard<std::mutex> lock(mutex_); return active_; }
RtspStats FakeRtspServer::stats() const { std::lock_guard<std::mutex> lock(mutex_); return stats_; }
std::size_t FakeRtspServer::start_count() const { std::lock_guard<std::mutex> lock(mutex_); return start_count_; }
std::size_t FakeRtspServer::stop_count() const { std::lock_guard<std::mutex> lock(mutex_); return stop_count_; }
CameraFormat FakeRtspServer::last_format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_format_;
}

}  // namespace cockpit::media
