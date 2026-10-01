#include "cockpit_ui/core_integration_runtime.h"
#include "cockpit_ui/main_window.h"
#include "cockpit_ui/mock_ui_backend.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QTimer>

#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RK3576 AI Cockpit"));
    QApplication::setOrganizationName(QStringLiteral("rk3576-ai-cockpit"));

    const auto arguments = app.arguments();
    QString backend_name = QStringLiteral("mock");
    QString profile_name = QStringLiteral("normal");
    QString media_backend_name = QStringLiteral("mock");
    QString camera_device;
    QString snapshot_directory;
    QString recording_directory;
    QString start_page_name = QStringLiteral("home");
    for (int index = 1; index < arguments.size(); ++index) {
        const auto& argument = arguments.at(index);
        if (argument.startsWith(QStringLiteral("--backend="))) {
            backend_name = argument.mid(10);
        } else if (argument == QStringLiteral("--backend") && index + 1 < arguments.size()) {
            backend_name = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--profile="))) {
            profile_name = argument.mid(10);
        } else if (argument == QStringLiteral("--profile") && index + 1 < arguments.size()) {
            profile_name = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--media-backend="))) {
            media_backend_name = argument.mid(16);
        } else if (argument == QStringLiteral("--media-backend") && index + 1 < arguments.size()) {
            media_backend_name = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--camera-device="))) {
            camera_device = argument.mid(16);
        } else if (argument == QStringLiteral("--camera-device") && index + 1 < arguments.size()) {
            camera_device = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--snapshot-dir="))) {
            snapshot_directory = argument.mid(15);
        } else if (argument == QStringLiteral("--snapshot-dir") && index + 1 < arguments.size()) {
            snapshot_directory = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--recording-dir="))) {
            recording_directory = argument.mid(16);
        } else if (argument == QStringLiteral("--recording-dir") && index + 1 < arguments.size()) {
            recording_directory = arguments.at(++index);
        } else if (argument.startsWith(QStringLiteral("--start-page="))) {
            start_page_name = argument.mid(13);
        } else if (argument == QStringLiteral("--start-page") && index + 1 < arguments.size()) {
            start_page_name = arguments.at(++index);
        }
    }

    std::unique_ptr<cockpit::ui::CoreIntegrationRuntime> core_runtime;
    std::unique_ptr<cockpit::ui::IUiBackend> backend;
    if (backend_name == QStringLiteral("mock")) {
        backend = std::make_unique<cockpit::ui::MockUiBackend>();
    } else if (backend_name == QStringLiteral("core")) {
        cockpit::ui::CoreDemoProfile profile;
        if (!cockpit::ui::parseCoreDemoProfile(profile_name.toStdString(), profile)) {
            qCritical() << "Unknown --profile" << profile_name
                        << "(expected normal, media-failure, media-timeout, rtos-offline)";
            return 2;
        }
        cockpit::ui::CoreIntegrationRuntimeOptions options;
        options.profile = profile;
        if (media_backend_name == QStringLiteral("mock")) {
            options.media_backend = cockpit::ui::MediaBackendKind::Mock;
            qInfo() << "MEDIA_BACKEND=MOCK";
        } else if (media_backend_name == QStringLiteral("cam0")) {
            if (profile != cockpit::ui::CoreDemoProfile::Normal || camera_device.isEmpty() ||
                snapshot_directory.isEmpty()) {
                qCritical() << "CAM0 real mode requires --profile normal, --camera-device and --snapshot-dir";
                return 2;
            }
            options.media_backend = cockpit::ui::MediaBackendKind::Cam0Real;
            options.camera.device = camera_device.toStdString();
            options.camera.camera_id = "front";
            options.snapshot_directory = snapshot_directory.toStdString();
            options.recording_directory = recording_directory.isEmpty()
                                              ? std::string("/home/cat/cockpit/recordings")
                                              : recording_directory.toStdString();
            qInfo() << "MEDIA_BACKEND=CAM0_REAL device=" << camera_device
                    << "snapshot_dir=" << snapshot_directory
                    << "recording_dir="
                    << QString::fromStdString(options.recording_directory);
        } else {
            qCritical() << "Unknown --media-backend" << media_backend_name
                        << "(expected mock or cam0)";
            return 2;
        }
        core_runtime = std::make_unique<cockpit::ui::CoreIntegrationRuntime>(std::move(options));
        if (!core_runtime->start()) {
            qCritical() << "Vehicle Core runtime failed to start";
            return 3;
        }
        backend = core_runtime->makeUiBackend();
    } else {
        qCritical() << "Unknown --backend" << backend_name << "(expected mock or core)";
        return 2;
    }

    const auto preview_mailbox = core_runtime ? core_runtime->previewMailbox() : nullptr;
    auto window = std::make_unique<cockpit::ui::MainWindow>(std::move(backend), preview_mailbox);

    if (arguments.contains(QStringLiteral("--windowed"))) {
        window->show();
    } else {
        window->showFullScreen();
    }

    cockpit::ui::PageId start_page = cockpit::ui::PageId::Count;
    for (const auto page : cockpit::ui::kPageOrder) {
        const auto name = cockpit::ui::pageName(page);
        if (start_page_name.compare(
                QString::fromUtf8(name.data(), static_cast<int>(name.size())),
                Qt::CaseInsensitive) == 0) {
            start_page = page;
            break;
        }
    }
    if (start_page == cockpit::ui::PageId::Count) {
        qCritical() << "Unknown --start-page" << start_page_name;
        return 2;
    }
    window->showPage(start_page);

    constexpr char quit_prefix[] = "--quit-after-ms=";
    for (const auto& argument : arguments) {
        if (!argument.startsWith(QString::fromLatin1(quit_prefix))) {
            continue;
        }
        bool valid = false;
        const auto delay = argument.mid(static_cast<int>(sizeof(quit_prefix) - 1)).toInt(&valid);
        if (valid && delay > 0) {
            QTimer::singleShot(delay, &app, [&app] { app.quit(); });
        }
    }
    const int exit_code = app.exec();
    const auto preview_stats = window->previewStats();
    window.reset();
    if (core_runtime && core_runtime->mediaService()) {
        core_runtime->stop();
        const auto capture = core_runtime->mediaService()->capture_stats();
        const auto elapsed_ns = capture.last_dequeue_steady_ns - capture.first_dequeue_steady_ns;
        const double elapsed = elapsed_ns > 0
                                   ? static_cast<double>(elapsed_ns) / 1'000'000'000.0
                                   : 0.0;
        const double capture_fps = elapsed > 0.0 && capture.frames > 1
                                       ? static_cast<double>(capture.frames - 1) / elapsed
                                       : 0.0;
        qInfo() << "CAM0_CAPTURE_METRICS frames=" << capture.frames
                << "fps=" << capture_fps
                << "sequence_gaps=" << capture.sequence_gap_count
                << "dqbuf_errors=" << capture.dequeue_errors
                << "qbuf_errors=" << capture.queue_errors
                << "poll_timeouts=" << capture.poll_timeouts;
        qInfo() << "CAM0_PREVIEW_METRICS converted=" << preview_stats.converted_frames
                << "delivered=" << preview_stats.delivered_frames
                << "fps=" << preview_stats.displayed_fps
                << "mailbox_drops=" << preview_stats.mailbox_drops
                << "ui_drops=" << preview_stats.ui_drops
                << "throttled=" << preview_stats.throttled_frames;
    }
    return exit_code;
}
