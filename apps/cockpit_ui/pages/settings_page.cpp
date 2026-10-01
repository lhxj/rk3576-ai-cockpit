#include "pages/settings_page.h"

#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace cockpit::ui {

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 12, 18, 12);
    root->setSpacing(12);

    auto* heading = new QLabel(QStringLiteral("Settings · UI ONLY"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    root->addWidget(heading);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(14);
    auto* brightness = new QSlider(Qt::Horizontal, this);
    auto* volume = new QSlider(Qt::Horizontal, this);
    brightness->setValue(50);
    volume->setValue(50);
    brightness->setEnabled(false);
    volume->setEnabled(false);
    form->addRow(QStringLiteral("Brightness placeholder"), brightness);
    form->addRow(QStringLiteral("Volume placeholder"), volume);
    form->addRow(QStringLiteral("Camera preferences"), new QLabel(QStringLiteral("NOT READY"), this));
    form->addRow(QStringLiteral("AI preferences"), new QLabel(QStringLiteral("NOT READY"), this));
    form->addRow(QStringLiteral("About"),
                 new QLabel(QStringLiteral("RK3576 AI Cockpit · Foundation MVP"), this));
    root->addLayout(form);
    root->addStretch();

    state_note_ = new QLabel(
        QStringLiteral("No brightness, mixer, network, boot, or kernel setting is changed."), this);
    state_note_->setWordWrap(true);
    state_note_->setStyleSheet("background: #172231; border-radius: 8px; padding: 10px;");
    root->addWidget(state_note_);
}

void SettingsPage::setState(const UiState& state) {
    state_note_->setText(
        QStringLiteral("UI-only placeholders. Latest backend RESULT: %1\n"
                       "No brightness, mixer, network, boot, or kernel setting is changed.")
            .arg(QString::fromStdString(state.latest_result)));
}

}  // namespace cockpit::ui

