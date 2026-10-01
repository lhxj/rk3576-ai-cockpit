#include "cockpit/media/fake_camera_capture.hpp"
#include "cockpit/media/media_service.hpp"
#include "cockpit/media/nv12_rgb.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

#define CHECK(expression)                                                                    \
    do {                                                                                     \
        if (!(expression)) {                                                                 \
            std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #expression << '\n';   \
            return 1;                                                                        \
        }                                                                                    \
    } while (false)

namespace {

cockpit::media::MediaOperationResult submit_and_wait(
    cockpit::media::MediaService& service, cockpit::media::MediaOperation operation) {
    auto promise = std::make_shared<std::promise<cockpit::media::MediaOperationResult>>();
    auto future = promise->get_future();
    const auto accepted = service.submit(operation, [promise](auto result) {
        promise->set_value(std::move(result));
    });
    if (!accepted.ok()) return {accepted, {}, {}};
    if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
        return {{cockpit::media::MediaStatusCode::Timeout, "test wait timeout"}, {}, {}};
    return future.get();
}

cockpit::media::MediaServiceConfig config(const std::filesystem::path& directory) {
    cockpit::media::MediaServiceConfig value;
    value.capture.device = "fake-cam0";
    value.capture.camera_id = "front";
    value.capture.width = 8;
    value.capture.height = 4;
    value.capture.fps = 30;
    value.capture.buffer_count = 4;
    value.snapshot_directory = directory.string();
    return value;
}

}  // namespace

int main() {
    using namespace cockpit::media;
    const auto directory = std::filesystem::temp_directory_path() /
                           ("cockpit-media-test-" + std::to_string(
                               std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);

    auto fake = std::make_unique<FakeCameraCapture>();
    auto* fake_ptr = fake.get();
    auto mailbox = std::make_shared<PreviewMailbox>();
    MediaService service(config(directory), std::move(fake), mailbox);
    CHECK(service.start().ok());

    const auto stopped_snapshot = submit_and_wait(service, MediaOperation::Snapshot);
    CHECK(stopped_snapshot.status.code == MediaStatusCode::CameraNotStreaming);

    const auto start1 = submit_and_wait(service, MediaOperation::PreviewStart);
    CHECK(start1.status.ok());
    PreviewDelivery delivery1;
    CHECK(mailbox->wait_next(0, std::chrono::seconds(1), delivery1));
    CHECK(delivery1.frame && delivery1.frame->stream_epoch == 1);
    const auto first_payload = delivery1.frame->payload;
    CHECK(!first_payload.empty());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto fake_source = fake_ptr->last_source_buffer();
    CHECK(!fake_source.empty());
    CHECK(delivery1.frame->payload == first_payload);
    CHECK(delivery1.frame->payload != fake_source);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CHECK(mailbox->drop_count() > 0);

    const auto snapshot = submit_and_wait(service, MediaOperation::Snapshot);
    CHECK(snapshot.status.ok() && snapshot.frame && !snapshot.output_path.empty());
    CHECK(std::filesystem::exists(snapshot.output_path));
    std::ifstream ppm(snapshot.output_path, std::ios::binary);
    std::string magic;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t maximum = 0;
    ppm >> magic >> width >> height >> maximum;
    CHECK(magic == "P6" && width == 8 && height == 4 && maximum == 255);

    const auto stop1 = submit_and_wait(service, MediaOperation::PreviewStop);
    CHECK(stop1.status.ok() && !service.streaming());
    const auto stop_again = submit_and_wait(service, MediaOperation::PreviewStop);
    CHECK(stop_again.status.ok());
    const auto start2 = submit_and_wait(service, MediaOperation::PreviewStart);
    CHECK(start2.status.ok());
    PreviewDelivery delivery2;
    CHECK(mailbox->wait_next(delivery1.delivery_id, std::chrono::seconds(1), delivery2));
    CHECK(delivery2.frame && delivery2.frame->stream_epoch == 2);
    CHECK(submit_and_wait(service, MediaOperation::PreviewStart).status.ok());
    CHECK(submit_and_wait(service, MediaOperation::PreviewStop).status.ok());
    service.stop();

    PreviewEpochFilter filter;
    CapturedFrame e10;
    e10.stream_epoch = 10;
    e10.sequence = 100;
    CHECK(filter.accept(e10));
    CapturedFrame e11;
    e11.stream_epoch = 11;
    e11.sequence = 1;
    CHECK(filter.accept(e11));
    e10.sequence = 101;
    CHECK(!filter.accept(e10));
    CHECK(filter.current_epoch() == 11 && filter.stale_drop_count() == 1);

    CapturedFrame invalid;
    invalid.pixel_format = "NV12";
    invalid.width = 8;
    invalid.height = 4;
    invalid.bytes_per_line = 8;
    invalid.bytes_used = 1;
    invalid.payload.resize(1);
    std::vector<std::uint8_t> rgb;
    CHECK(nv12_to_rgb888(invalid, rgb).code == MediaStatusCode::InvalidArgument);

    FakeCameraCaptureOptions fail_options;
    fail_options.fail_open = true;
    MediaService failing(config(directory),
                         std::make_unique<FakeCameraCapture>(fail_options));
    CHECK(failing.start().ok());
    CHECK(submit_and_wait(failing, MediaOperation::PreviewStart).status.code ==
          MediaStatusCode::Unavailable);
    failing.stop();

    std::filesystem::remove_all(directory);
    std::cout << "media_service_test: PASS\n";
    return 0;
}
