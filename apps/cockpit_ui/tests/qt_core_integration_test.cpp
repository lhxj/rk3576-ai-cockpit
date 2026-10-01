#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>

#include <chrono>
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

template <typename Predicate>
bool processUntil(Predicate predicate,
                  std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < end) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (predicate()) return true;
        std::this_thread::yield();
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
    using cockpit::ui::CoreIntegrationRuntime;
    using cockpit::ui::MainWindow;
    using cockpit::vehicle::CommandType;

    CoreIntegrationRuntime runtime;
    CHECK(runtime.start());
    {
        MainWindow window(runtime.makeUiBackend());
        window.show();
        CHECK(processUntil([&] {
            auto* mode = window.findChild<QLabel*>(QStringLiteral("backendMode"));
            return mode != nullptr && mode->text().contains(QStringLiteral("CORE"));
        }));

        CHECK(button(window, "nav_camera") != nullptr);
        button(window, "nav_camera")->click();
        CHECK(button(window, "camera_recording") != nullptr);
        button(window, "camera_recording")->click();
        CHECK(processUntil([&] {
            return runtime.mediaAdapter()->invocation_count(CommandType::RECORDING_START) == 1;
        }));
        auto* camera_result = window.findChild<QLabel*>(QStringLiteral("camera_result"));
        CHECK(camera_result != nullptr);
        CHECK(processUntil([&] { return camera_result->text().contains(QStringLiteral("completed")); }));

        CHECK(button(window, "camera_rtsp") != nullptr);
        button(window, "camera_rtsp")->click();
        CHECK(processUntil([&] {
            return runtime.mediaAdapter()->invocation_count(CommandType::RTSP_START) == 1;
        }));

        CHECK(button(window, "camera_rear") != nullptr);
        button(window, "camera_rear")->click();
        CHECK(processUntil([&] {
            return camera_result->text().contains(QStringLiteral("unavailable"),
                                                  Qt::CaseInsensitive);
        }));

        CHECK(button(window, "nav_media") != nullptr);
        button(window, "nav_media")->click();
        CHECK(button(window, "media_play") != nullptr);
        button(window, "media_play")->click();
        CHECK(processUntil([&] {
            return runtime.mediaAdapter()->invocation_count(CommandType::MEDIA_PLAY) == 1;
        }));

        CHECK(button(window, "nav_vehicle") != nullptr);
        button(window, "nav_vehicle")->click();
        CHECK(button(window, "vehicle_led") != nullptr);
        button(window, "vehicle_led")->click();
        CHECK(processUntil([&] {
            return runtime.rtosAdapter()->invocation_count(CommandType::SIM_LED_SET) == 1;
        }));
        auto* vehicle_result = window.findChild<QLabel*>(QStringLiteral("vehicle_result"));
        CHECK(vehicle_result != nullptr);
        CHECK(processUntil([&] {
            return vehicle_result->text().contains(QStringLiteral("SIMULATED"));
        }));

        CHECK(button(window, "nav_ai") != nullptr);
        button(window, "nav_ai")->click();
        CHECK(button(window, "voice_session") != nullptr);
        button(window, "voice_session")->click();
        CHECK(processUntil([&] {
            return runtime.voiceAdapter()->invocation_count(CommandType::VOICE_SESSION_START) == 1;
        }));
    }
    runtime.stop();
    std::cout << "qt_core_integration_test: PASS\n";
    return 0;
}
