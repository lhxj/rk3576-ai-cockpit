#include "pages/vehicle_page.h"

#include "widgets/status_badge.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>
#include <QTimer>

namespace cockpit::ui {

VehiclePage::VehiclePage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 10);
    root->setSpacing(10);

    auto* header = new QHBoxLayout;
    heading_ = new QLabel(QStringLiteral("Vehicle / Sensor · MOCK"), this);
    heading_->setStyleSheet("font-size: 19px; font-weight: 700;");
    rtos_status_ = new StatusBadge(QStringLiteral("RTOS"), this);
    sensor_status_ = new StatusBadge(QStringLiteral("MPU6050"), this);
    header->addWidget(heading_);
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
        values_[index]=cell;
        cell->setObjectName(QStringLiteral("sensor_value_%1").arg(index));
        values->addWidget(cell, index / 3, index % 3);
    }
    root->addLayout(values, 1);
    chip_temp_=new QLabel(QStringLiteral("MPU chip temperature: -- °C"),this);
    chip_temp_->setObjectName("sensor_chip_temp");root->addWidget(chip_temp_);
    sensor_detail_=new QLabel(QStringLiteral("NO_DATA / source=UNKNOWN / module XYZ axes"),this);
    sensor_detail_->setObjectName("sensor_detail");sensor_detail_->setWordWrap(true);root->addWidget(sensor_detail_);
    auto* timer=new QTimer(this);timer->setInterval(100);
    connect(timer,&QTimer::timeout,this,&VehiclePage::renderSensor);timer->start();

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
        QStringLiteral("LED and buzzer controls are software simulations"),
        this);
    result_->setObjectName(QStringLiteral("vehicle_result"));
    result_->setWordWrap(true);
    root->addWidget(result_);

    connect(led_button_, &QPushButton::toggled, this, &VehiclePage::ledRequested);
    connect(buzzer_button_, &QPushButton::toggled, this, &VehiclePage::buzzerRequested);
}

void VehiclePage::setState(const UiState& state) {
    if(sensor_dirty_)++ui_merges_;
    pending_sensor_=state.sensor_values;sensor_dirty_=true;
    rtos_status_->setStatus(state.rtos);
    sensor_status_->setStatus(state.sensor);
    controls_status_->setStatus(state.simulated_controls);
    const QSignalBlocker led_blocker(led_button_);
    const QSignalBlocker buzzer_blocker(buzzer_button_);
    led_button_->setChecked(state.simulated_led_on);
    buzzer_button_->setChecked(state.simulated_buzzer_on);
}

void VehiclePage::renderSensor() {
    if(!sensor_dirty_)return;
    sensor_dirty_=false;
    const auto& s=pending_sensor_;
    const char* data="NO_DATA";
    switch(s.data){
    case vehicle::SensorDataCondition::VALID:data="VALID";break;
    case vehicle::SensorDataCondition::STALE:data="STALE";break;
    case vehicle::SensorDataCondition::OFFLINE:data="OFFLINE";break;
    case vehicle::SensorDataCondition::ERROR:data="ERROR";break;
    case vehicle::SensorDataCondition::NO_DATA:break;
    }
    const auto source=s.source==vehicle::StateSource::RUNTIME?QStringLiteral("RUNTIME"):
        (s.source==vehicle::StateSource::MOCK?QStringLiteral("MOCK"):QStringLiteral("UNKNOWN"));
    heading_->setText(QStringLiteral("Vehicle / Sensor · %1").arg(source));
    const QStringList names{"Accel X","Accel Y","Accel Z","Gyro X","Gyro Y","Gyro Z"};
    for(int i=0;i<6;++i){
        const auto number=s.has_value?QString::number(i<3?s.accel_g[i]:s.gyro_dps[i-3],'f',3):QStringLiteral("--");
        values_[i]->setText(QStringLiteral("%1\n%2 %3 / %4").arg(names[i],number,i<3?QStringLiteral("g"):QStringLiteral("°/s"),QString::fromLatin1(data)));
    }
    chip_temp_->setText(QStringLiteral("MPU chip temperature: %1 °C / %2").arg(s.has_value?QString::number(s.chip_temp_c,'f',2):QStringLiteral("--"),QString::fromLatin1(data)));
    sensor_detail_->setText(QStringLiteral("%1 · source=%2 · module XYZ axes\nRTOS=%3 RPMsg=%4 MPU=%5 · sample=%6 publish=%7 age=%8 ms · UI merges=%9")
        .arg(QString::fromLatin1(data),source).arg(s.rtos_online).arg(s.rpmsg_online).arg(s.mpu_available)
        .arg(s.sample_seq).arg(s.publish_seq).arg(s.has_value?QString::number(s.age_ms):QStringLiteral("--")).arg(ui_merges_));
}

void VehiclePage::showResult(const UiResult& result) {
    result_->setText(QStringLiteral("RESULT #%1: %2")
                         .arg(static_cast<qulonglong>(result.request_id))
                         .arg(QString::fromStdString(result.message)));
    result_->setStyleSheet(result.succeeded() ? "color: #8dd9b0;" : "color: #ff9aa6;");
}

}  // namespace cockpit::ui
