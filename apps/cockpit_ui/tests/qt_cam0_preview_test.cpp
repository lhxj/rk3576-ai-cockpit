#include "cockpit/media/fake_camera_capture.hpp"
#include "cockpit/media/fake_h264_encoder.hpp"
#include "cockpit/media/fake_rtsp_server.hpp"
#include "cockpit/media/file_recording_sink.hpp"
#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QVariant>

#include <chrono>
#include <filesystem>
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
using namespace std::chrono_literals;

template <typename Predicate>
bool process_until(Predicate predicate, std::chrono::milliseconds timeout = 2s) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (predicate()) return true;
        std::this_thread::sleep_for(1ms);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    return predicate();
}

QPushButton* button(cockpit::ui::MainWindow& window, const char* name) {
    return window.findChild<QPushButton*>(QString::fromLatin1(name));
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    using cockpit::media::FakeCameraCapture;
    using cockpit::media::FakeCameraCaptureOptions;
    using cockpit::media::FakeH264Encoder;
    using cockpit::ui::CoreIntegrationRuntime;
    using cockpit::ui::CoreIntegrationRuntimeOptions;
    using cockpit::ui::MainWindow;
    using cockpit::ui::MediaBackendKind;
    using cockpit::vehicle::PreviewState;
    using cockpit::vehicle::RecordingState;
    using cockpit::vehicle::BinaryState;

    const auto output = std::filesystem::temp_directory_path() /
                        ("cockpit-qt-cam0-" + std::to_string(
                            std::chrono::steady_clock::now().time_since_epoch().count()));
    CoreIntegrationRuntimeOptions options;
    options.media_backend = MediaBackendKind::Cam0Real;
    options.camera.device = "fake-cam0";
    options.camera.camera_id = "front";
    options.camera.width = 8;
    options.camera.height = 4;
    options.camera.pixel_format = "NV12";
    options.camera.fps = 30;
    options.camera.buffer_count = 4;
    options.snapshot_directory = output.string();
    options.recording_directory = output.string();
    CoreIntegrationRuntime runtime(
        std::move(options),
        std::make_unique<FakeCameraCapture>(
            FakeCameraCaptureOptions{false, false, false, false, 10ms}),
        std::make_unique<FakeH264Encoder>(),
        std::make_unique<cockpit::media::FileRecordingSink>(),
        std::make_unique<cockpit::media::FakeRtspServer>());
    CHECK(runtime.start());
    {
        MainWindow window(runtime.makeUiBackend(), runtime.previewMailbox());
        window.show();
        auto* mode = window.findChild<QLabel*>(QStringLiteral("backendMode"));
        CHECK(mode != nullptr);
        CHECK(process_until([&] { return mode->text().contains(QStringLiteral("CAM0 REAL")); }));
        CHECK(button(window, "nav_camera") != nullptr);
        button(window, "nav_camera")->click();

        auto* preview = window.findChild<QLabel*>(QStringLiteral("camera_preview_image"));
        CHECK(preview != nullptr);
        const bool received_preview = process_until([&] {
            return preview->property("previewFrameWidth").toInt() == 8 &&
                   preview->property("previewFrameHeight").toInt() == 4 &&
                   preview->property("previewFrameEpoch").toULongLong() == 1;
        });
        if (!received_preview) {
            const auto state = runtime.client().get_snapshot();
            const auto stats = runtime.mediaService()->capture_stats();
            std::cerr << "preview diagnostic state=" << static_cast<int>(state.preview.value)
                      << " condition=" << static_cast<int>(state.preview.condition)
                      << " frames=" << stats.frames
                      << " epoch=" << stats.stream_epoch
                      << " property_width=" << preview->property("previewFrameWidth").toInt()
                      << " property_epoch=" << preview->property("previewFrameEpoch").toULongLong()
                      << '\n';
        }
        CHECK(received_preview);
        CHECK(preview->property("previewUpdatedOnGuiThread").toBool());
        CHECK(!preview->pixmap(Qt::ReturnByValue).isNull());
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().preview.value == PreviewState::STREAMING;
        }));

        auto* result = window.findChild<QLabel*>(QStringLiteral("camera_result"));
        CHECK(result != nullptr);
        CHECK(process_until([&] {
            return result->text().contains(QStringLiteral("preview start"),
                                           Qt::CaseInsensitive);
        }));
        CHECK(button(window, "camera_recording") != nullptr);
        button(window, "camera_recording")->click();
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().recording.value ==
                       RecordingState::RECORDING &&
                   result->text().contains(QStringLiteral("recording"),
                                           Qt::CaseInsensitive);
        }));
        button(window, "camera_recording")->click();
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().recording.value ==
                       RecordingState::STOPPED &&
                   runtime.client().get_snapshot().preview.value ==
                       PreviewState::STREAMING &&
                   runtime.mediaService()->recorder_stats().file_closed;
        }));

        CHECK(button(window, "camera_rtsp") != nullptr);
        button(window, "camera_rtsp")->click();
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().rtsp.value == BinaryState::ON &&
                   runtime.client().get_snapshot().rtsp.source ==
                       cockpit::vehicle::StateSource::RUNTIME &&
                   runtime.mediaService()->rtsp_active() &&
                   button(window, "camera_rtsp")->isEnabled() &&
                   button(window, "camera_rtsp")->text() == QStringLiteral("Stop RTSP");
        }));
        button(window, "camera_rtsp")->click();
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().rtsp.value == BinaryState::OFF &&
                   !runtime.mediaService()->rtsp_active() &&
                   runtime.client().get_snapshot().preview.value == PreviewState::STREAMING;
        }));

        CHECK(button(window, "camera_snapshot") != nullptr);
        button(window, "camera_snapshot")->click();
        CHECK(process_until([&] {
            return result->text().contains(QStringLiteral("snapshot"), Qt::CaseInsensitive) &&
                   result->text().contains(QStringLiteral("path="), Qt::CaseInsensitive);
        }));

        CHECK(button(window, "nav_home") != nullptr);
        button(window, "nav_home")->click();
        CHECK(process_until([&] {
            return runtime.client().get_snapshot().preview.value == PreviewState::STOPPED;
        }));
        button(window, "nav_camera")->click();
        CHECK(process_until([&] {
            return preview->property("previewFrameEpoch").toULongLong() == 2 &&
                   runtime.client().get_snapshot().preview.value == PreviewState::STREAMING;
        }));
    }
    runtime.stop();
    CHECK(!runtime.core().running());
    std::error_code ignored;
    std::filesystem::remove_all(output, ignored);
    std::cout << "qt_cam0_preview_test: PASS\n";
    return 0;
}
