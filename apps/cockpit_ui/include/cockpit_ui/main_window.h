#pragma once

#include "cockpit_ui/page_id.h"
#include "cockpit_ui/qt_preview_bridge.h"
#include "cockpit_ui/ui_backend.h"

#include "cockpit/media/preview_mailbox.hpp"

#include <QMainWindow>

#include <memory>
#include <vector>

class QStackedWidget;
class QLabel;

namespace cockpit::ui {

class AiPage;
class CameraPage;
class HomePage;
class MediaPage;
class MonitorPage;
class NavigationButton;
class SettingsPage;
class StatusBadge;
class VehiclePage;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::unique_ptr<IUiBackend> backend,
                        std::shared_ptr<media::PreviewMailbox> preview_mailbox = {},
                        QWidget* parent = nullptr);
    ~MainWindow() override;
    void showPage(PageId page);
    [[nodiscard]] QtPreviewStats previewStats() const;

private:
    void buildUi();
    void connectActions();
    void navigate(PageId page);
    void applyState(const UiState& state);
    void applyResult(const UiResult& result);
    UiResult dispatch(UiCommand command, std::string argument = {}, bool enabled = false);

    std::unique_ptr<IUiBackend> backend_;
    std::unique_ptr<QtPreviewBridge> preview_bridge_;
    QStackedWidget* stack_{nullptr};
    QLabel* backend_mode_{nullptr};
    StatusBadge* wifi_status_{nullptr};
    StatusBadge* rtos_status_{nullptr};
    HomePage* home_page_{nullptr};
    CameraPage* camera_page_{nullptr};
    MediaPage* media_page_{nullptr};
    VehiclePage* vehicle_page_{nullptr};
    AiPage* ai_page_{nullptr};
    MonitorPage* monitor_page_{nullptr};
    SettingsPage* settings_page_{nullptr};
    std::vector<NavigationButton*> navigation_buttons_;
    PageId current_page_{PageId::Home};
};

}  // namespace cockpit::ui
