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
        core_runtime = std::make_unique<cockpit::ui::CoreIntegrationRuntime>(profile);
        if (!core_runtime->start()) {
            qCritical() << "Vehicle Core runtime failed to start";
            return 3;
        }
        backend = core_runtime->makeUiBackend();
    } else {
        qCritical() << "Unknown --backend" << backend_name << "(expected mock or core)";
        return 2;
    }

    cockpit::ui::MainWindow window(std::move(backend));

    if (arguments.contains(QStringLiteral("--windowed"))) {
        window.show();
    } else {
        window.showFullScreen();
    }

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
    return app.exec();
}
