#include "cockpit_ui/main_window.h"
#include "cockpit_ui/mock_ui_backend.h"

#include <QApplication>
#include <QStringList>

#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RK3576 AI Cockpit"));
    QApplication::setOrganizationName(QStringLiteral("rk3576-ai-cockpit"));

    auto backend = std::make_unique<cockpit::ui::MockUiBackend>();
    cockpit::ui::MainWindow window(std::move(backend));

    if (app.arguments().contains(QStringLiteral("--windowed"))) {
        window.show();
    } else {
        window.showFullScreen();
    }
    return app.exec();
}

