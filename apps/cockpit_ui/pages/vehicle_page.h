#pragma once

#include "cockpit_ui/ui_backend.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace cockpit::ui {

class StatusBadge;

class VehiclePage final : public QWidget {
    Q_OBJECT

public:
    explicit VehiclePage(QWidget* parent = nullptr);
    void setState(const UiState& state);
    void showResult(const UiResult& result);

signals:
    void ledRequested(bool enabled);
    void buzzerRequested(bool enabled);

private:
    StatusBadge* rtos_status_{nullptr};
    StatusBadge* sensor_status_{nullptr};
    StatusBadge* controls_status_{nullptr};
    QLabel* result_{nullptr};
    QPushButton* led_button_{nullptr};
    QPushButton* buzzer_button_{nullptr};
};

}  // namespace cockpit::ui
