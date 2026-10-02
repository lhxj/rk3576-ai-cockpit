#include "cockpit/media/fake_camera_capture.hpp"
#include "cockpit/media/fake_h264_encoder.hpp"
#include "cockpit/media/fake_rtsp_server.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/real_media_service_adapter.hpp"
#include "cockpit/media/rtp_h264.hpp"
#include "cockpit/media/rtsp_server.hpp"
#include "cockpit/vehicle/client.hpp"
#include "cockpit/vehicle/clock.hpp"
#include "cockpit/vehicle/core.hpp"
#include "cockpit/vehicle/service_adapter.hpp"
#include "cockpit/vehicle/service_registry.hpp"

#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #expression << '\n'; return 1; \
} } while (false)

namespace {
using namespace std::chrono_literals;
namespace media = cockpit::media;
namespace vehicle = cockpit::vehicle;
namespace protocol = cockpit::protocol;

template <typename Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = 2s) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

std::shared_ptr<media::EncodedPacket> access_unit(bool idr, std::size_t payload = 4,
                                                   std::uint32_t timestamp = 9000) {
    auto packet = std::make_shared<media::EncodedPacket>();
    const auto add = [&](std::uint8_t header, std::size_t size) {
        packet->annex_b.insert(packet->annex_b.end(), {0, 0, 0, 1, header});
        packet->annex_b.insert(packet->annex_b.end(), size, 0x55U);
    };
    if (idr) { add(0x67U, 5); add(0x68U, 3); add(0x65U, payload); }
    else add(0x41U, payload);
    packet->key_frame = idr;
    packet->rtp_timestamp = timestamp;
    return packet;
}

class FakeUdpTransport final : public media::IUdpTransport {
public:
    explicit FakeUdpTransport(std::chrono::milliseconds delay = 0ms) : delay_(delay) {}
    media::MediaStatus configure(const std::string&, std::uint16_t) override {
        configured_ = true; return media::MediaStatus::Ok();
    }
    media::MediaStatus send(const std::vector<std::uint8_t>& bytes) override {
        if (delay_.count() > 0) std::this_thread::sleep_for(delay_);
        std::lock_guard<std::mutex> lock(mutex_);
        datagrams_.push_back(bytes);
        return media::MediaStatus::Ok();
    }
    void close() override { configured_ = false; }
    std::uint16_t local_port() const override { return 50000; }
    std::size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_); return datagrams_.size();
    }
private:
    std::chrono::milliseconds delay_;
    mutable std::mutex mutex_;
    std::vector<std::vector<std::uint8_t>> datagrams_;
    bool configured_{false};
};

int connect_rtsp(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    (void)::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

std::string request(int fd, const std::string& text) {
    if (::send(fd, text.data(), text.size(), MSG_NOSIGNAL) !=
        static_cast<ssize_t>(text.size())) return {};
    std::vector<char> buffer(8192);
    const auto size = ::recv(fd, buffer.data(), buffer.size(), 0);
    return size > 0 ? std::string(buffer.data(), static_cast<std::size_t>(size)) : std::string{};
}

media::MediaServiceConfig config(const std::filesystem::path& output) {
    media::MediaServiceConfig value;
    value.capture.device = "fake-cam0";
    value.capture.camera_id = "front";
    value.capture.width = 16;
    value.capture.height = 8;
    value.capture.pixel_format = "NV12";
    value.capture.fps = 30;
    value.capture.buffer_count = 4;
    value.snapshot_directory = output.string();
    value.recording_directory = output.string();
    value.encoder.first_packet_timeout = 1s;
    value.rtsp.port = 0;
    value.rtsp.path = "/cam0";
    return value;
}

vehicle::VehicleCommand command(vehicle::VehicleCore& core, vehicle::IClock& clock,
                                protocol::RequestId id, vehicle::CommandType type,
                                protocol::Deadline delta = 2000) {
    vehicle::VehicleCommand value;
    value.request_id = id;
    value.boot_epoch = core.boot_epoch();
    value.deadline_ms = clock.now_ms() + delta;
    value.source = vehicle::CommandSource::TEST;
    value.command_type = type;
    return value;
}

vehicle::CommandResult result(vehicle::CommandSubmission& submission) {
    if (submission.result.wait_for(3s) != std::future_status::ready)
        throw std::runtime_error("result timeout");
    return submission.result.get();
}

struct Fixture {
    Fixture(const std::filesystem::path& output,
            media::FakeH264EncoderOptions encoder_options = {},
            std::shared_ptr<vehicle::IClock> supplied_clock =
                std::make_shared<vehicle::SystemClock>(),
            media::FakeRtspServerOptions rtsp_options = {})
        : clock(std::move(supplied_clock)),
          camera(std::make_unique<media::FakeCameraCapture>()), camera_ptr(camera.get()),
          encoder(std::make_unique<media::FakeH264Encoder>(encoder_options)),
          encoder_ptr(encoder.get()),
          rtsp(std::make_unique<media::FakeRtspServer>(rtsp_options)), rtsp_ptr(rtsp.get()),
          service(std::make_shared<media::MediaService>(
              config(output), std::move(camera), std::make_shared<media::PreviewMailbox>(),
              std::move(encoder), std::make_unique<media::FileRecordingSink>(),
              std::move(rtsp))),
          adapter(std::make_shared<media::RealMediaServiceAdapter>(service)),
          voice(std::make_shared<vehicle::MockVoiceAdapter>()),
          rtos(std::make_shared<vehicle::MockRtosAdapter>()),
          system(std::make_shared<vehicle::MockSystemAdapter>()),
          registry(std::make_shared<vehicle::ServiceRegistry>()),
          core({20261002, 32, 32, 128, 1ms}, {adapter, voice, rtos, system},
               registry, clock), client(core) {
        registry->set(vehicle::ServiceDomain::MEDIA, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::RUNTIME);
        registry->set(vehicle::ServiceDomain::VOICE, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::RTOS, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        registry->set(vehicle::ServiceDomain::SYSTEM, vehicle::ServiceHealth::ONLINE,
                      vehicle::StateSource::MOCK);
        adapter->set_runtime_state_callback([this](vehicle::CommandType type,
                                                    vehicle::AdapterResult result_value) {
            (void)core.report_runtime_result(type, std::move(result_value));
        });
    }
    ~Fixture() { core.stop(); service->stop(); }
    void start() {
        if (!service->start().ok() || !core.start().ok())
            throw std::runtime_error("fixture start");
    }
    std::shared_ptr<vehicle::IClock> clock;
    std::unique_ptr<media::FakeCameraCapture> camera;
    media::FakeCameraCapture* camera_ptr;
    std::unique_ptr<media::FakeH264Encoder> encoder;
    media::FakeH264Encoder* encoder_ptr;
    std::unique_ptr<media::FakeRtspServer> rtsp;
    media::FakeRtspServer* rtsp_ptr;
    std::shared_ptr<media::MediaService> service;
    std::shared_ptr<media::RealMediaServiceAdapter> adapter;
    std::shared_ptr<vehicle::MockVoiceAdapter> voice;
    std::shared_ptr<vehicle::MockRtosAdapter> rtos;
    std::shared_ptr<vehicle::MockSystemAdapter> system;
    std::shared_ptr<vehicle::ServiceRegistry> registry;
    vehicle::VehicleCore core;
    vehicle::InProcessVehicleCoreClient client;
};

}  // namespace

int main() {
    // RFC 6184 single-NAL and FU-A packetization, common timestamp and final marker.
    media::H264RtpPacketizer packetizer(8);
    std::uint16_t sequence = 100;
    auto small = packetizer.packetize(*access_unit(false, 2, 9000), sequence);
    CHECK(small.size() == 1 && small.front().sequence == 100);
    CHECK(small.front().timestamp == 9000 && small.front().marker);
    auto fragmented = packetizer.packetize(*access_unit(false, 25, 12000), sequence);
    CHECK(fragmented.size() > 1 && (fragmented.front().bytes[12] & 0x1FU) == 28U);
    CHECK((fragmented.front().bytes[13] & 0x80U) != 0U);
    CHECK((fragmented.back().bytes[13] & 0x40U) != 0U && fragmented.back().marker);
    for (std::size_t i = 1; i < fragmented.size(); ++i)
        CHECK(fragmented[i].sequence == fragmented[i - 1].sequence + 1U);
    for (const auto& packet : fragmented) CHECK(packet.timestamp == 12000);

    // SPS/PPS cache, wait-for-IDR, bounded slow-network queue and resync.
    auto transport = std::make_unique<FakeUdpTransport>(20ms);
    auto* transport_ptr = transport.get();
    media::RtpSender sender(std::move(transport), 6, 1200);
    CHECK(sender.start().ok());
    CHECK(sender.configure_client("127.0.0.1", 5004).ok());
    sender.begin_play();
    CHECK(sender.submit(access_unit(false)).ok());
    std::this_thread::sleep_for(20ms);
    CHECK(transport_ptr->count() == 0);
    CHECK(sender.submit(access_unit(true)).ok());
    CHECK(wait_until([&] { return transport_ptr->count() >= 3; }));
    CHECK(sender.parameters_ready());
    const auto first_join_ms = sender.stats().client_join_to_first_idr_ms;
    CHECK(first_join_ms >= 0);
    for (int i = 0; i < 20; ++i) CHECK(sender.submit(access_unit(false)).ok());
    CHECK(wait_until([&] { return sender.stats().rtp_drop_count > 0; }));
    const auto before_resync = transport_ptr->count();
    CHECK(sender.submit(access_unit(false)).ok());
    CHECK(sender.submit(access_unit(true, 2, 18000)).ok());
    CHECK(wait_until([&] { return transport_ptr->count() > before_resync; }));
    CHECK(sender.stats().queue_peak_depth <= 6);
    CHECK(sender.stats().client_join_to_first_idr_ms == first_join_ms);
    sender.stop();

    // Minimal real RTSP control server: methods, SDP, UDP SETUP, PLAY and reconnect.
    auto control_transport = std::make_unique<FakeUdpTransport>();
    auto* control_transport_ptr = control_transport.get();
    media::RtspServer server(std::move(control_transport));
    media::RtspConfig rtsp_config;
    rtsp_config.bind_address = "127.0.0.1";
    rtsp_config.port = 0;
    rtsp_config.path = "/cam0";
    media::CameraFormat stream_format{"front", 1632, 1224, "H264", 1, 0, 0,
                                      0, 0, 30, 1, true, "fake"};
    std::atomic_uint idr_requests{0};
    CHECK(server.start(rtsp_config, stream_format, [&] { ++idr_requests; }).ok());
    CHECK(server.submit(access_unit(true)).ok());
    CHECK(server.wait_for_parameters(1s).ok());
    const auto listen_port = server.stats().listen_port;
    CHECK(listen_port != 0);
    for (int connection = 0; connection < 2; ++connection) {
        const int fd = connect_rtsp(listen_port);
        CHECK(fd >= 0);
        CHECK(request(fd, "OPTIONS rtsp://127.0.0.1/cam0 RTSP/1.0\r\nCSeq: 1\r\n\r\n")
                  .find("200 OK") != std::string::npos);
        CHECK(request(fd,
            "DESCRIBE rtsp://127.0.0.1/wrong RTSP/1.0\r\nCSeq: 11\r\nAccept: application/sdp\r\n\r\n")
                  .find("404 Not Found") != std::string::npos);
        const auto describe = request(fd,
            "DESCRIBE rtsp://127.0.0.1/cam0 RTSP/1.0\r\nCSeq: 2\r\nAccept: application/sdp\r\n\r\n");
        CHECK(describe.find("H264/90000") != std::string::npos);
        CHECK(describe.find("sprop-parameter-sets=") != std::string::npos);
        CHECK(describe.find("a=framerate:30") != std::string::npos);
        CHECK(request(fd,
            "SETUP rtsp://127.0.0.1/cam0/trackID=0 RTSP/1.0\r\nCSeq: 12\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n")
                  .find("461 Unsupported Transport") != std::string::npos);
        const auto setup = request(fd,
            "SETUP rtsp://127.0.0.1/cam0/trackID=0 RTSP/1.0\r\nCSeq: 3\r\nTransport: RTP/AVP/UDP;unicast;client_port=5004-5005\r\n\r\n");
        CHECK(setup.find("server_port=50000-50001") != std::string::npos);
        CHECK(request(fd, "PLAY rtsp://127.0.0.1/cam0 RTSP/1.0\r\nCSeq: 4\r\nSession: 35760001\r\n\r\n")
                  .find("200 OK") != std::string::npos);
        CHECK(server.submit(access_unit(false)).ok());
        CHECK(server.submit(access_unit(true)).ok());
        CHECK(wait_until([&] { return control_transport_ptr->count() >=
                                     static_cast<std::size_t>((connection + 1) * 3); }));
        CHECK(request(fd, "TEARDOWN rtsp://127.0.0.1/cam0 RTSP/1.0\r\nCSeq: 5\r\nSession: 35760001\r\n\r\n")
                  .find("200 OK") != std::string::npos);
        ::close(fd);
    }
    CHECK(idr_requests.load() == 2);
    CHECK(server.stats().client_reconnect_count == 1);
    CHECK(server.stop().ok());
    CHECK(!server.active());

    // Stopping the server must wake an idle connected client without a double close.
    media::RtspServer shutdown_server;
    CHECK(shutdown_server.start(rtsp_config, stream_format, {}).ok());
    const int idle_fd = connect_rtsp(shutdown_server.stats().listen_port);
    CHECK(idle_fd >= 0);
    auto shutdown = std::async(std::launch::async, [&] { return shutdown_server.stop(); });
    CHECK(shutdown.wait_for(1s) == std::future_status::ready);
    CHECK(shutdown.get().ok());
    ::close(idle_fd);

    const auto output = std::filesystem::temp_directory_path() /
        ("cockpit-rtsp-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(output);
    Fixture fixture(output);
    fixture.start();

    auto recording = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 1, vehicle::CommandType::RECORDING_START));
    CHECK(recording.accepted() && result(recording).status.ok());
    auto rtsp = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 2, vehicle::CommandType::RTSP_START));
    CHECK(rtsp.accepted());
    const auto rtsp_ack_sequence = rtsp.ack.lifecycle_sequence;
    const auto rtsp_result = result(rtsp);
    CHECK(rtsp_result.status.ok());
    CHECK(rtsp_ack_sequence < rtsp_result.lifecycle_sequence);
    CHECK(fixture.service->recording_active() && fixture.service->rtsp_active());
    CHECK(fixture.service->encoder_stats().start_count == 1);
    CHECK(fixture.camera_ptr->start_count() == 1);
    CHECK(fixture.rtsp_ptr->last_format().fps_numerator == 30);
    CHECK(fixture.rtsp_ptr->last_format().fps_denominator == 1);
    CHECK(fixture.client.get_snapshot().rtsp.source == vehicle::StateSource::RUNTIME);

    const auto rtsp_requests = fixture.service->service_stats().rtsp_start_requests;
    auto duplicate = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 3, vehicle::CommandType::RTSP_START));
    CHECK(duplicate.accepted() && result(duplicate).status.ok());
    CHECK(fixture.service->service_stats().rtsp_start_requests == rtsp_requests);

    auto stop_recording = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 4, vehicle::CommandType::RECORDING_STOP));
    CHECK(stop_recording.accepted() && result(stop_recording).status.ok());
    CHECK(!fixture.service->recording_active() && fixture.service->rtsp_active());
    CHECK(fixture.service->streaming() && fixture.encoder_ptr->active());
    CHECK(fixture.service->encoder_stats().start_count == 1);

    auto stop_rtsp = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 5, vehicle::CommandType::RTSP_STOP));
    CHECK(stop_rtsp.accepted() && result(stop_rtsp).status.ok());
    CHECK(!fixture.service->rtsp_active() && !fixture.service->streaming());
    CHECK(!fixture.encoder_ptr->active());

    auto rtsp_first = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 6, vehicle::CommandType::RTSP_START));
    CHECK(rtsp_first.accepted() && result(rtsp_first).status.ok());
    auto recording_second = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 7, vehicle::CommandType::RECORDING_START));
    CHECK(recording_second.accepted() && result(recording_second).status.ok());
    CHECK(fixture.service->encoder_stats().start_count == 2);
    auto rtsp_stop_first = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 8, vehicle::CommandType::RTSP_STOP));
    CHECK(rtsp_stop_first.accepted() && result(rtsp_stop_first).status.ok());
    CHECK(fixture.service->recording_active() && fixture.encoder_ptr->active());
    auto recording_stop_last = fixture.client.send_command(command(
        fixture.core, *fixture.clock, 9, vehicle::CommandType::RECORDING_STOP));
    CHECK(recording_stop_last.accepted() && result(recording_stop_last).status.ok());
    CHECK(!fixture.service->streaming() && !fixture.encoder_ptr->active());

    // One sink failure must not tear down the other consumer or shared encoder.
    Fixture isolated(output / "sink-isolation", {},
                     std::make_shared<vehicle::SystemClock>(), {false, true});
    isolated.start();
    auto isolated_recording = isolated.client.send_command(command(
        isolated.core, *isolated.clock, 10, vehicle::CommandType::RECORDING_START));
    CHECK(isolated_recording.accepted() && result(isolated_recording).status.ok());
    auto failing_rtsp = isolated.client.send_command(command(
        isolated.core, *isolated.clock, 11, vehicle::CommandType::RTSP_START));
    CHECK(failing_rtsp.accepted() && result(failing_rtsp).status.ok());
    CHECK(wait_until([&] {
        return !isolated.service->rtsp_active() && isolated.service->recording_active() &&
               isolated.client.get_snapshot().rtsp.condition ==
                   vehicle::StateCondition::ERROR;
    }));
    CHECK(isolated.encoder_ptr->active());
    auto isolated_stop = isolated.client.send_command(command(
        isolated.core, *isolated.clock, 12, vehicle::CommandType::RECORDING_STOP));
    CHECK(isolated_stop.accepted() && result(isolated_stop).status.ok());
    isolated.core.stop();
    isolated.service->stop();

    auto fake_clock = std::make_shared<vehicle::FakeClock>(1000);
    media::FakeH264EncoderOptions delayed;
    delayed.encode_delay = 100ms;
    Fixture timeout(output / "timeout", delayed, fake_clock);
    timeout.start();
    auto late = timeout.client.send_command(command(
        timeout.core, *timeout.clock, 20, vehicle::CommandType::RTSP_START, 5));
    CHECK(late.accepted());
    CHECK(wait_until([&] {
        return timeout.client.get_snapshot().rtsp.condition == vehicle::StateCondition::STARTING;
    }));
    fake_clock->advance(6);
    CHECK(timeout.core.poll_deadlines().ok());
    CHECK(late.result.wait_for(1s) == std::future_status::ready);
    CHECK(late.result.get().status.code == protocol::StatusCode::TIMEOUT);
    const auto revision = timeout.client.get_snapshot().revision;
    CHECK(wait_until([&] { return timeout.core.ignored_late_results() >= 1; }));
    CHECK(timeout.client.get_snapshot().revision == revision);
    timeout.core.stop();
    timeout.service->stop();

    std::error_code ignored;
    std::filesystem::remove_all(output, ignored);
    std::cout << "media_rtsp_test: PASS\n";
    return 0;
}
