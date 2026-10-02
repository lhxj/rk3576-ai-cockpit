#include "cockpit/media/rtsp_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>

namespace cockpit::media {
namespace {

class PosixUdpTransport final : public IUdpTransport {
public:
    ~PosixUdpTransport() override { close(); }
    MediaStatus configure(const std::string& host, std::uint16_t port) override {
        std::lock_guard<std::mutex> lock(mutex_);
        close_locked();
        fd_ = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) return {MediaStatusCode::Unavailable, "create RTP socket"};
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0;
        if (::bind(fd_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
            const auto detail = std::string("bind RTP socket: ") + std::strerror(errno);
            close_locked();
            return {MediaStatusCode::Unavailable, detail};
        }
        remote_ = {};
        remote_.sin_family = AF_INET;
        remote_.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &remote_.sin_addr) != 1) {
            close_locked();
            return {MediaStatusCode::InvalidArgument, "RTP client IPv4"};
        }
        sockaddr_in actual{};
        socklen_t size = sizeof(actual);
        if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&actual), &size) == 0)
            local_port_ = ntohs(actual.sin_port);
        return MediaStatus::Ok();
    }
    MediaStatus send(const std::vector<std::uint8_t>& datagram) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fd_ < 0) return {MediaStatusCode::InvalidState, "RTP socket not configured"};
        const auto sent = ::sendto(fd_, datagram.data(), datagram.size(), MSG_DONTWAIT,
                                   reinterpret_cast<const sockaddr*>(&remote_), sizeof(remote_));
        if (sent < 0 || static_cast<std::size_t>(sent) != datagram.size())
            return {MediaStatusCode::Unavailable,
                    std::string("RTP sendto: ") + std::strerror(errno)};
        return MediaStatus::Ok();
    }
    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        close_locked();
    }
    std::uint16_t local_port() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return local_port_;
    }
private:
    void close_locked() {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
        local_port_ = 0;
    }
    mutable std::mutex mutex_;
    int fd_{-1};
    sockaddr_in remote_{};
    std::uint16_t local_port_{0};
};

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1U);
}

std::map<std::string, std::string> parse_headers(std::istringstream& input) {
    std::map<std::string, std::string> headers;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon != std::string::npos)
            headers[trim(line.substr(0, colon))] = trim(line.substr(colon + 1U));
    }
    return headers;
}

std::uint16_t client_rtp_port(const std::string& transport) {
    const auto marker = transport.find("client_port=");
    if (marker == std::string::npos) return 0;
    const auto begin = marker + std::strlen("client_port=");
    const auto end = transport.find_first_of("-;\r\n", begin);
    try {
        const auto value = std::stoul(transport.substr(begin, end - begin));
        return value < 65535U ? static_cast<std::uint16_t>(value) : 0;
    } catch (...) { return 0; }
}

std::string request_path(const std::string& target) {
    std::string path = target;
    constexpr const char* scheme = "rtsp://";
    if (path.rfind(scheme, 0) == 0) {
        const auto begin = path.find('/', std::strlen(scheme));
        path = begin == std::string::npos ? "/" : path.substr(begin);
    }
    const auto query = path.find_first_of("?#");
    if (query != std::string::npos) path.erase(query);
    while (path.size() > 1U && path.back() == '/') path.pop_back();
    return path;
}

bool is_udp_transport(const std::string& transport) {
    return transport.find("RTP/AVP") != std::string::npos &&
           transport.find("TCP") == std::string::npos &&
           transport.find("interleaved=") == std::string::npos;
}

bool send_all(int fd, const std::string& response) {
    std::size_t offset = 0;
    while (offset < response.size()) {
        const auto sent = ::send(fd, response.data() + offset, response.size() - offset,
                                 MSG_NOSIGNAL);
        if (sent <= 0) return false;
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

}  // namespace

RtpSender::RtpSender(std::unique_ptr<IUdpTransport> transport,
                     std::size_t queue_capacity, std::size_t max_payload)
    : transport_(std::move(transport)), packetizer_(max_payload),
      queue_capacity_(queue_capacity) {}
RtpSender::~RtpSender() { stop(); }

MediaStatus RtpSender::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return MediaStatus::Ok("RTP sender already running");
    if (!transport_ || queue_capacity_ == 0)
        return {MediaStatusCode::InvalidArgument, "RTP sender configuration"};
    stopping_ = false;
    running_ = true;
    try { worker_ = std::thread(&RtpSender::run, this); }
    catch (...) {
        running_ = false;
        return {MediaStatusCode::Unavailable, "RTP sender worker start"};
    }
    return MediaStatus::Ok();
}

MediaStatus RtpSender::configure_client(const std::string& host, std::uint16_t port) {
    const auto status = transport_->configure(host, port);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!status.ok()) { stats_.last_error = status.detail; return status; }
    configured_ = true;
    stats_.rtp_server_port = transport_->local_port();
    return MediaStatus::Ok();
}

void RtpSender::begin_play() {
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = configured_;
    waiting_for_idr_ = true;
    queue_.clear();
    client_join_time_ = std::chrono::steady_clock::now();
    stats_.client_join_to_first_idr_ms = -1;
}

void RtpSender::end_play() {
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = false;
    configured_ = false;
    waiting_for_idr_ = true;
    queue_.clear();
    transport_->close();
    stats_.rtp_server_port = 0;
}

std::vector<H264Nal> RtpSender::decodable_join_nals(const EncodedPacket& packet) const {
    std::vector<H264Nal> nals;
    if (!sps_.empty()) nals.push_back(sps_);
    if (!pps_.empty()) nals.push_back(pps_);
    for (const auto& nal : split_annex_b(packet.annex_b)) {
        if (nal.empty()) continue;
        const auto type = static_cast<std::uint8_t>(nal.front() & 0x1FU);
        if (type != 7U && type != 8U) nals.push_back(nal);
    }
    return nals;
}

MediaStatus RtpSender::submit(std::shared_ptr<const EncodedPacket> packet) {
    if (!packet) return {MediaStatusCode::InvalidArgument, "RTP access unit"};
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& nal : split_annex_b(packet->annex_b)) {
        if (nal.empty()) continue;
        const auto type = static_cast<std::uint8_t>(nal.front() & 0x1FU);
        if (type == 7U) sps_ = nal;
        else if (type == 8U) pps_ = nal;
    }
    ready_.notify_all();
    if (!playing_ || !configured_) return MediaStatus::Ok("RTP no active client");
    if (waiting_for_idr_ && !packet->key_frame) return MediaStatus::Ok("RTP waiting for IDR");
    const auto nals = waiting_for_idr_ ? decodable_join_nals(*packet)
                                       : split_annex_b(packet->annex_b);
    const auto sequence_before_packetization = next_sequence_;
    auto packets = packetizer_.packetize(nals, packet->rtp_timestamp, next_sequence_);
    if (packets.empty()) return MediaStatus::Ok("empty H.264 access unit");
    if (packets.size() > queue_capacity_) {
        stats_.rtp_drop_count += queue_.size() + packets.size();
        queue_.clear();
        waiting_for_idr_ = true;
        return MediaStatus::Ok("RTP access unit exceeds queue capacity");
    }
    if (queue_.size() + packets.size() > queue_capacity_) {
        stats_.rtp_drop_count += queue_.size();
        queue_.clear();
        waiting_for_idr_ = true;
        if (!packet->key_frame) {
            stats_.rtp_drop_count += packets.size();
            return MediaStatus::Ok("RTP queue overflow; waiting for IDR");
        }
        next_sequence_ = sequence_before_packetization;
        packets = packetizer_.packetize(decodable_join_nals(*packet),
                                        packet->rtp_timestamp, next_sequence_);
        if (packets.size() > queue_capacity_) {
            stats_.rtp_drop_count += packets.size();
            waiting_for_idr_ = true;
            return MediaStatus::Ok("RTP resync access unit exceeds queue capacity");
        }
    }
    for (auto& item : packets) queue_.push_back(std::move(item));
    stats_.queue_peak_depth = std::max(stats_.queue_peak_depth, queue_.size());
    if (waiting_for_idr_ && packet->key_frame) {
        waiting_for_idr_ = false;
        if (stats_.client_join_to_first_idr_ms < 0) {
            stats_.client_join_to_first_idr_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - client_join_time_).count();
        }
    }
    ready_.notify_one();
    return MediaStatus::Ok();
}

void RtpSender::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && !worker_.joinable()) return;
        stopping_ = true;
    }
    ready_.notify_all();
    if (worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    playing_ = false;
    configured_ = false;
    running_ = false;
    transport_->close();
}

bool RtpSender::parameters_ready() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !sps_.empty() && !pps_.empty();
}
MediaStatus RtpSender::wait_for_parameters(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!ready_.wait_for(lock, timeout, [&] {
            return (!sps_.empty() && !pps_.empty()) || stopping_ || !running_;
        })) return {MediaStatusCode::Timeout, "RTSP SPS/PPS timeout"};
    return !sps_.empty() && !pps_.empty()
               ? MediaStatus::Ok()
               : MediaStatus{MediaStatusCode::InvalidState, "RTP sender stopped"};
}
std::string RtpSender::sdp_fmtp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sps_.empty() || pps_.empty()) return {};
    return "packetization-mode=1;profile-level-id=" + h264_profile_level_id(sps_) +
           ";sprop-parameter-sets=" + base64_encode(sps_) + "," + base64_encode(pps_);
}
RtspStats RtpSender::stats() const { std::lock_guard<std::mutex> lock(mutex_); return stats_; }
std::uint16_t RtpSender::next_sequence() const { std::lock_guard<std::mutex> lock(mutex_); return next_sequence_; }

void RtpSender::run() {
    for (;;) {
        RtpPacket packet;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) { if (stopping_) break; continue; }
            packet = std::move(queue_.front());
            queue_.pop_front();
        }
        const auto status = transport_->send(packet.bytes);
        std::lock_guard<std::mutex> lock(mutex_);
        if (status.ok()) ++stats_.rtp_packet_count;
        else {
            ++stats_.rtp_drop_count;
            stats_.last_error = status.detail;
            stats_.rtp_drop_count += queue_.size();
            queue_.clear();
            waiting_for_idr_ = true;
        }
    }
}

struct RtspServer::Impl {
    explicit Impl(std::unique_ptr<IUdpTransport> udp) : udp_override(std::move(udp)) {}
    MediaStatus start_server(const RtspConfig& requested, const CameraFormat& input,
                             RequestIdrCallback idr) {
        std::lock_guard<std::mutex> lock(mutex);
        if (active) return MediaStatus::Ok("RTSP already active");
        if (requested.path.empty() || requested.path.front() != '/' ||
            requested.packet_queue_capacity == 0 || requested.max_rtp_payload < 3U ||
            requested.max_rtp_payload > 65'500U)
            return {MediaStatusCode::InvalidArgument, "RTSP configuration"};
        config = requested;
        while (config.path.size() > 1U && config.path.back() == '/')
            config.path.pop_back();
        format = input;
        final_stats = {};
        request_idr = std::move(idr);
        listen_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listen_fd < 0) return {MediaStatusCode::Unavailable, "create RTSP socket"};
        int reuse = 1;
        (void)::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(config.port);
        if (config.bind_address == "0.0.0.0") address.sin_addr.s_addr = htonl(INADDR_ANY);
        else if (::inet_pton(AF_INET, config.bind_address.c_str(), &address.sin_addr) != 1) {
            close_sockets();
            return {MediaStatusCode::InvalidArgument, "RTSP bind IPv4"};
        }
        if (::bind(listen_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listen_fd, 1) != 0) {
            const auto detail = std::string("bind/listen RTSP: ") + std::strerror(errno);
            close_sockets();
            return {MediaStatusCode::Unavailable, detail};
        }
        sockaddr_in actual{};
        socklen_t size = sizeof(actual);
        if (::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&actual), &size) == 0)
            listen_port = ntohs(actual.sin_port);
        auto transport = udp_override ? std::move(udp_override)
                                      : std::make_unique<PosixUdpTransport>();
        sender = std::make_unique<RtpSender>(std::move(transport),
                                             config.packet_queue_capacity,
                                             config.max_rtp_payload);
        auto status = sender->start();
        if (!status.ok()) { close_sockets(); sender.reset(); return status; }
        stopping = false;
        active = true;
        try { control = std::thread(&Impl::control_loop, this); }
        catch (...) {
            active = false;
            sender->stop(); sender.reset(); close_sockets();
            return {MediaStatusCode::Unavailable, "RTSP control worker start"};
        }
        return MediaStatus::Ok();
    }

    void close_sockets() {
        if (client_fd >= 0) { ::shutdown(client_fd, SHUT_RDWR); ::close(client_fd); }
        if (listen_fd >= 0) { ::shutdown(listen_fd, SHUT_RDWR); ::close(listen_fd); }
        client_fd = -1;
        listen_fd = -1;
    }

    MediaStatus stop_server() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active && !control.joinable()) return MediaStatus::Ok("RTSP already stopped");
            stopping = true;
            if (client_fd >= 0) (void)::shutdown(client_fd, SHUT_RDWR);
            if (listen_fd >= 0) (void)::shutdown(listen_fd, SHUT_RDWR);
        }
        if (control.joinable()) control.join();
        if (sender) sender->stop();
        std::lock_guard<std::mutex> lock(mutex);
        close_sockets();
        if (sender) final_stats = sender->stats();
        final_stats.listen_port = listen_port;
        final_stats.client_connect_count = connect_count;
        final_stats.client_reconnect_count = reconnect_count;
        final_stats.client_disconnect_count = disconnect_count;
        sender.reset();
        active = false;
        return MediaStatus::Ok();
    }

    std::string response(unsigned code, const std::string& reason,
                         const std::string& cseq, const std::string& extra = {},
                         const std::string& body = {}) const {
        std::ostringstream out;
        out << "RTSP/1.0 " << code << ' ' << reason << "\r\nCSeq: " << cseq << "\r\n"
            << "Server: rk3576-cockpit/1\r\n";
        if (!extra.empty()) out << extra;
        if (!body.empty()) out << "Content-Length: " << body.size() << "\r\n";
        out << "\r\n" << body;
        return out.str();
    }

    std::string sdp() const {
        std::ostringstream out;
        out << "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\ns=RK3576 CAM0\r\n"
            << "t=0 0\r\na=control:*\r\nm=video 0 RTP/AVP 96\r\n"
            << "c=IN IP4 0.0.0.0\r\na=rtpmap:96 H264/90000\r\n"
            << "a=fmtp:96 " << sender->sdp_fmtp() << "\r\n"
            << "a=framesize:96 " << format.width << '-' << format.height << "\r\n"
            << "a=framerate:"
            << (format.fps_denominator == 0
                    ? 0.0
                    : static_cast<double>(format.fps_numerator) /
                          static_cast<double>(format.fps_denominator))
            << "\r\n"
            << "a=control:trackID=0\r\n";
        return out.str();
    }

    void control_loop() {
        for (;;) {
            int accepted = -1;
            int server_fd = -1;
            sockaddr_in peer{};
            socklen_t peer_size = sizeof(peer);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping || listen_fd < 0) break;
                server_fd = listen_fd;
            }
            accepted = ::accept4(server_fd, reinterpret_cast<sockaddr*>(&peer),
                                 &peer_size, SOCK_CLOEXEC);
            if (accepted < 0) {
                if (errno == EINTR) continue;
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) break;
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) { ::close(accepted); break; }
                client_fd = accepted;
                ++connect_count;
                if (connect_count > 1) ++reconnect_count;
            }
            char peer_text[INET_ADDRSTRLEN]{};
            (void)::inet_ntop(AF_INET, &peer.sin_addr, peer_text, sizeof(peer_text));
            bool teardown = false;
            std::string buffered;
            while (!teardown) {
                std::array<char, 4096> bytes{};
                const auto count = ::recv(accepted, bytes.data(), bytes.size(), 0);
                if (count <= 0) break;
                buffered.append(bytes.data(), static_cast<std::size_t>(count));
                std::size_t end = 0;
                while ((end = buffered.find("\r\n\r\n")) != std::string::npos) {
                    const auto request = buffered.substr(0, end + 4U);
                    buffered.erase(0, end + 4U);
                    std::istringstream input(request);
                    std::string method, target, version;
                    input >> method >> target >> version;
                    std::string ignored;
                    std::getline(input, ignored);
                    const auto headers = parse_headers(input);
                    const auto found_cseq = headers.find("CSeq");
                    const auto cseq = found_cseq == headers.end() ? "0" : found_cseq->second;
                    const auto path = request_path(target);
                    const auto track_path = config.path == "/"
                        ? std::string("/trackID=0") : config.path + "/trackID=0";
                    std::string answer;
                    if (method == "OPTIONS") {
                        answer = response(200, "OK", cseq,
                            "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n");
                    } else if (method == "DESCRIBE") {
                        if (path != config.path)
                            answer = response(404, "Not Found", cseq);
                        else if (!sender->parameters_ready())
                            answer = response(503, "Service Unavailable", cseq,
                                              "Retry-After: 1\r\n");
                        else {
                            const auto body = sdp();
                            answer = response(200, "OK", cseq,
                                "Content-Type: application/sdp\r\nContent-Base: " + target + "/\r\n",
                                body);
                        }
                    } else if (method == "SETUP") {
                        const auto found = headers.find("Transport");
                        const auto valid_transport = found != headers.end() &&
                                                     is_udp_transport(found->second);
                        const auto port = !valid_transport ? 0 : client_rtp_port(found->second);
                        const auto status = path != track_path
                            ? MediaStatus{MediaStatusCode::InvalidArgument, "RTSP path"}
                            : port == 0
                            ? MediaStatus{MediaStatusCode::InvalidArgument, "client_port"}
                            : sender->configure_client(peer_text, port);
                        if (path != track_path) answer = response(404, "Not Found", cseq);
                        else if (!status.ok()) answer = response(461, "Unsupported Transport", cseq);
                        else {
                            const auto server_port = sender->stats().rtp_server_port;
                            answer = response(200, "OK", cseq,
                                "Session: 35760001\r\nTransport: RTP/AVP;unicast;client_port=" +
                                std::to_string(port) + '-' + std::to_string(port + 1U) +
                                ";server_port=" + std::to_string(server_port) + '-' +
                                std::to_string(server_port + 1U) + "\r\n");
                        }
                    } else if (method == "PLAY") {
                        if (path != config.path) answer = response(404, "Not Found", cseq);
                        else {
                            sender->begin_play();
                            if (request_idr) request_idr();
                            auto base = target;
                            while (!base.empty() && base.back() == '/') base.pop_back();
                            answer = response(200, "OK", cseq,
                                "Session: 35760001\r\nRTP-Info: url=" + base +
                                "/trackID=0;seq=" + std::to_string(sender->next_sequence()) + "\r\n");
                        }
                    } else if (method == "TEARDOWN") {
                        if (path != config.path && path != track_path)
                            answer = response(404, "Not Found", cseq);
                        else {
                            sender->end_play();
                            answer = response(200, "OK", cseq, "Session: 35760001\r\n");
                            teardown = true;
                        }
                    } else answer = response(405, "Method Not Allowed", cseq);
                    if (!send_all(accepted, answer)) { teardown = true; break; }
                }
            }
            sender->end_play();
            bool close_accepted = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (client_fd == accepted) {
                    client_fd = -1;
                    close_accepted = true;
                }
                ++disconnect_count;
                if (stopping) {
                    if (close_accepted) ::close(accepted);
                    break;
                }
            }
            if (close_accepted) ::close(accepted);
        }
    }

    mutable std::mutex mutex;
    RtspConfig config;
    CameraFormat format;
    RequestIdrCallback request_idr;
    std::unique_ptr<IUdpTransport> udp_override;
    std::unique_ptr<RtpSender> sender;
    std::thread control;
    int listen_fd{-1};
    int client_fd{-1};
    std::uint16_t listen_port{0};
    std::uint64_t connect_count{0};
    std::uint64_t reconnect_count{0};
    std::uint64_t disconnect_count{0};
    RtspStats final_stats;
    bool active{false};
    bool stopping{false};
};

RtspServer::RtspServer() : impl_(std::make_unique<Impl>(nullptr)) {}
RtspServer::RtspServer(std::unique_ptr<IUdpTransport> transport)
    : impl_(std::make_unique<Impl>(std::move(transport))) {}
RtspServer::~RtspServer() { (void)impl_->stop_server(); }
MediaStatus RtspServer::start(const RtspConfig& config, const CameraFormat& format,
                              RequestIdrCallback request_idr) {
    return impl_->start_server(config, format, std::move(request_idr));
}
MediaStatus RtspServer::submit(std::shared_ptr<const EncodedPacket> packet) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->active || !impl_->sender)
        return {MediaStatusCode::InvalidState, "RTSP server stopped"};
    return impl_->sender->submit(std::move(packet));
}
MediaStatus RtspServer::wait_for_parameters(std::chrono::milliseconds timeout) {
    RtpSender* sender = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->active || !impl_->sender)
            return {MediaStatusCode::InvalidState, "RTSP server stopped"};
        sender = impl_->sender.get();
    }
    return sender->wait_for_parameters(timeout);
}
MediaStatus RtspServer::stop() { return impl_->stop_server(); }
bool RtspServer::active() const { std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->active; }
RtspStats RtspServer::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    RtspStats stats = impl_->sender ? impl_->sender->stats() : impl_->final_stats;
    stats.listen_port = impl_->listen_port;
    stats.client_connect_count = impl_->connect_count;
    stats.client_reconnect_count = impl_->reconnect_count;
    stats.client_disconnect_count = impl_->disconnect_count;
    return stats;
}
void RtspServer::control_loop() { impl_->control_loop(); }

}  // namespace cockpit::media
