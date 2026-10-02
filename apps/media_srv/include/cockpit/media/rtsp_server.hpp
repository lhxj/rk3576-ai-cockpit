#pragma once

#include "cockpit/media/h264_encoder.hpp"
#include "cockpit/media/rtp_h264.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cockpit::media {

struct RtspConfig {
    std::string bind_address{"0.0.0.0"};
    std::uint16_t port{8554};
    std::string path{"/cam0"};
    std::size_t packet_queue_capacity{1024};
    std::size_t max_rtp_payload{1200};
};

struct RtspStats {
    std::uint64_t rtp_packet_count{0};
    std::uint64_t rtp_drop_count{0};
    std::size_t queue_peak_depth{0};
    std::uint64_t client_connect_count{0};
    std::uint64_t client_reconnect_count{0};
    std::uint64_t client_disconnect_count{0};
    std::int64_t client_join_to_first_idr_ms{-1};
    std::uint16_t listen_port{0};
    std::uint16_t rtp_server_port{0};
    std::string last_error;
};

class IUdpTransport {
public:
    virtual ~IUdpTransport() = default;
    virtual MediaStatus configure(const std::string& host, std::uint16_t port) = 0;
    virtual MediaStatus send(const std::vector<std::uint8_t>& datagram) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual std::uint16_t local_port() const = 0;
};

class RtpSender {
public:
    RtpSender(std::unique_ptr<IUdpTransport> transport,
              std::size_t queue_capacity, std::size_t max_payload);
    ~RtpSender();
    MediaStatus start();
    MediaStatus configure_client(const std::string& host, std::uint16_t port);
    void begin_play();
    void end_play();
    MediaStatus submit(std::shared_ptr<const EncodedPacket> packet);
    void stop();
    [[nodiscard]] bool parameters_ready() const;
    MediaStatus wait_for_parameters(std::chrono::milliseconds timeout);
    [[nodiscard]] std::string sdp_fmtp() const;
    [[nodiscard]] RtspStats stats() const;
    [[nodiscard]] std::uint16_t next_sequence() const;

private:
    void run();
    std::vector<H264Nal> decodable_join_nals(const EncodedPacket& packet) const;
    std::unique_ptr<IUdpTransport> transport_;
    H264RtpPacketizer packetizer_;
    const std::size_t queue_capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<RtpPacket> queue_;
    std::thread worker_;
    RtspStats stats_;
    H264Nal sps_;
    H264Nal pps_;
    std::uint16_t next_sequence_{1};
    std::chrono::steady_clock::time_point client_join_time_{};
    bool running_{false};
    bool stopping_{false};
    bool configured_{false};
    bool playing_{false};
    bool waiting_for_idr_{true};
};

using RequestIdrCallback = std::function<void()>;

class IRtspServer {
public:
    virtual ~IRtspServer() = default;
    virtual MediaStatus start(const RtspConfig& config, const CameraFormat& format,
                              RequestIdrCallback request_idr) = 0;
    virtual MediaStatus submit(std::shared_ptr<const EncodedPacket> packet) = 0;
    virtual MediaStatus wait_for_parameters(std::chrono::milliseconds timeout) = 0;
    virtual MediaStatus stop() = 0;
    [[nodiscard]] virtual bool active() const = 0;
    [[nodiscard]] virtual RtspStats stats() const = 0;
};

class RtspServer final : public IRtspServer {
public:
    RtspServer();
    explicit RtspServer(std::unique_ptr<IUdpTransport> transport);
    ~RtspServer() override;
    MediaStatus start(const RtspConfig& config, const CameraFormat& format,
                      RequestIdrCallback request_idr) override;
    MediaStatus submit(std::shared_ptr<const EncodedPacket> packet) override;
    MediaStatus wait_for_parameters(std::chrono::milliseconds timeout) override;
    MediaStatus stop() override;
    [[nodiscard]] bool active() const override;
    [[nodiscard]] RtspStats stats() const override;

private:
    void control_loop();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cockpit::media
