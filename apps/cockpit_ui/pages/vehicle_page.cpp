#include "pages/vehicle_page.h"

#include "widgets/status_badge.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

namespace cockpit::ui {

VehiclePage::VehiclePage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 10);
    root->setSpacing(10);

    auto* header = new QHBoxLayout;
    auto* heading = new QLabel(QStringLiteral("Vehicle / Sensor · MOCK"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    rtos_status_ = new StatusBadge(QStringLiteral("RTOS"), this);
    sensor_status_ = new StatusBadge(QStringLiteral("MPU6050"), this);
    header->addWidget(heading);
    header->addStretch();
    header->addWidget(rtos_status_);
    header->addWidget(sensor_status_);
    root->addLayout(header);

    auto* values = new QGridLayout;
    values->setSpacing(8);
    const QStringList labels{QStringLiteral("Accel X"), QStringLiteral("Accel Y"),
                             QStringLiteral("Accel Z"), QStringLiteral("Gyro X"),
                             QStringLiteral("Gyro Y"), QStringLiteral("Gyro Z")};
    for (int index = 0; index < labels.size(); ++index) {
        auto* cell = new QLabel(QStringLiteral("%1\n-- / N/A").arg(labels.at(index)), this);
        cell->setAlignment(Qt::AlignCenter);
        cell->setMinimumHeight(62);
        cell->setStyleSheet("background: #1c2939; border-radius: 8px; font-size: 15px;");
        values->addWidget(cell, index / 3, index % 3);
    }
    root->addLayout(values, 1);

    controls_status_ = new StatusBadge(QStringLiteral("Controls"), this);
    root->addWidget(controls_status_);
    auto* controls = new QHBoxLayout;
    led_button_ = new QPushButton(QStringLiteral("LED · SIMULATED"), this);
    buzzer_button_ = new QPushButton(QStringLiteral("Buzzer · SIMULATED"), this);
    led_button_->setObjectName(QStringLiteral("vehicle_led"));
    buzzer_button_->setObjectName(QStringLiteral("vehicle_buzzer"));
    for (auto* button : {led_button_, buzzer_button_}) {
        button->setCheckable(true);
        button->setMinimumHeight(50);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    controls->addWidget(led_button_);
    controls->addWidget(buzzer_button_);
    root->addLayout(controls);

    result_ = new QLabel(
        QStringLiteral("Target path: UI → vehicle_core → RPMsg → RT-Thread → RESULT → UI\n"
                       "Current path: MOCK only; AMP is not verified"),
        this);
    result_->setObjectName(QStringLiteral("vehicle_result"));
    result_->setWordWrap(true);
    root->addWidget(result_);

    connect(led_button_, &QPushButton::toggled, this, &VehiclePage::ledRequested);
    connect(buzzer_button_, &QPushButton::toggled, this, &VehiclePage::buzzerRequested);
}

void VehiclePage::setState(const UiState& state) {
    rtos_status_->setStatus(state.rtos);
    sensor_status_->setStatus(state.sensor);
    controls_status_->setStatus(state.simulated_controls);
    const QSignalBlocker led_blocker(led_button_);
    const QSignalBlocker buzzer_blocker(buzzer_button_);
    led_button_->setChecked(state.simulated_led_on);
    buzzer_button_->setChecked(state.simulated_buzzer_on);
}

void VehiclePage::showResult(const UiResult& result) {
    result_->setText(QStringLiteral("RESULT #%1: %2")
                         .arg(static_cast<qulonglong>(result.request_id))
                         .arg(QString::fromStdString(result.message)));
    result_->setStyleSheet(result.succeeded() ? "color: #8dd9b0;" : "color: #ff9aa6;");
}

}  // namespace cockpit::ui
