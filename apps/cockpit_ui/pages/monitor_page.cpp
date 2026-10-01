#include "pages/monitor_page.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace cockpit::ui {
namespace {

QLabel* metric(const QString& title, QWidget* parent) {
    auto* label = new QLabel(QStringLiteral("%1\n-- · MOCK").arg(title), parent);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(70);
    label->setStyleSheet("background: #1c2939; border-radius: 9px; font-size: 16px;");
    return label;
}

QString statusLine(const QString& name, const ServiceStatus& status) {
    return QStringLiteral("%1: %2 / %3")
        .arg(name,
             QString::fromUtf8(toString(status.state).data(),
                               static_cast<int>(toString(status.state).size())),
             QString::fromUtf8(toString(status.source).data(),
                               static_cast<int>(toString(status.source).size())));
}

}  // namespace

MonitorPage::MonitorPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 10);
    root->setSpacing(10);

    auto* heading = new QLabel(QStringLiteral("Monitor · STATIC / DEMO DATA"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    root->addWidget(heading);

    auto* metrics = new QGridLayout;
    metrics->setSpacing(10);
    cpu_ = metric(QStringLiteral("CPU"), this);
    memory_ = metric(QStringLiteral("Memory"), this);
    temperature_ = metric(QStringLiteral("Temperature"), this);
    disk_ = metric(QStringLiteral("Disk"), this);
    metrics->addWidget(cpu_, 0, 0);
    metrics->addWidget(memory_, 0, 1);
    metrics->addWidget(temperature_, 1, 0);
    metrics->addWidget(disk_, 1, 1);
    root->addLayout(metrics, 1);

    service_summary_ = new QLabel(this);
    service_summary_->setWordWrap(true);
    service_summary_->setStyleSheet("background: #172231; border-radius: 8px; padding: 10px;");
    root->addWidget(service_summary_);
}

void MonitorPage::setState(const UiState& state) {
    cpu_->setText(QStringLiteral("CPU\n%1 · MOCK").arg(QString::fromStdString(state.cpu)));
    memory_->setText(QStringLiteral("Memory\n%1 · MOCK").arg(QString::fromStdString(state.memory)));
    temperature_->setText(
        QStringLiteral("Temperature\n%1 · MOCK").arg(QString::fromStdString(state.temperature)));
    disk_->setText(QStringLiteral("Disk\n%1 · MOCK").arg(QString::fromStdString(state.disk)));
    service_summary_->setText(
        statusLine(QStringLiteral("Wi-Fi"), state.wifi) + QStringLiteral("    ") +
        statusLine(QStringLiteral("Camera"), state.camera_front) + QStringLiteral("    ") +
        statusLine(QStringLiteral("Audio"), state.audio) + QStringLiteral("\n") +
        statusLine(QStringLiteral("RTOS"), state.rtos) + QStringLiteral("    ") +
        statusLine(QStringLiteral("Vision"), state.vision) + QStringLiteral("    ") +
        statusLine(QStringLiteral("Voice"), state.voice));
}

}  // namespace cockpit::ui
