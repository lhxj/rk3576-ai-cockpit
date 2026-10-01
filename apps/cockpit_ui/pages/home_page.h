#pragma once

#include "cockpit_ui/page_id.h"
#include "cockpit_ui/ui_state.h"

#include <QWidget>

namespace cockpit::ui {

class StatusBadge;

class HomePage final : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);
    void setState(const UiState& state);

signals:
    void navigateRequested(cockpit::ui::PageId page);

private:
    StatusBadge* camera_status_{nullptr};
    StatusBadge* audio_status_{nullptr};
    StatusBadge* ai_status_{nullptr};
    StatusBadge* vehicle_status_{nullptr};
};

}  // namespace cockpit::ui

