#include "cockpit_ui/main_window.h"
#include "cockpit_ui/mock_ui_backend.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStringList>
#include <QTimer>

#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RK3576 AI Cockpit"));
    QApplication::setOrganizationName(QStringLiteral("rk3576-ai-cockpit"));

    auto backend = std::make_unique<cockpit::ui::MockUiBackend>();
    cockpit::ui::MainWindow window(std::move(backend));

    const auto arguments = app.arguments();
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
