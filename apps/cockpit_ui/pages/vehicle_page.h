#pragma once

#include "cockpit_ui/ui_backend.h"

#include <QWidget>
#include <array>

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
    void renderSensor();
    std::array<QLabel*,6> values_{};
    QLabel* heading_{nullptr};
    QLabel* chip_temp_{nullptr};
    QLabel* sensor_detail_{nullptr};
    vehicle::SensorState pending_sensor_;
    bool sensor_dirty_{false};
    std::uint64_t ui_merges_{0};
    StatusBadge* rtos_status_{nullptr};
    StatusBadge* sensor_status_{nullptr};
    StatusBadge* controls_status_{nullptr};
    QLabel* result_{nullptr};
    QPushButton* led_button_{nullptr};
    QPushButton* buzzer_button_{nullptr};
};

}  // namespace cockpit::ui
