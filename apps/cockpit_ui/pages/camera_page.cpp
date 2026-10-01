#include "pages/camera_page.h"

#include "widgets/status_badge.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace cockpit::ui {
namespace {

QPushButton* actionButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(48);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

}  // namespace

CameraPage::CameraPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(8);

    auto* heading = new QLabel(QStringLiteral("Camera · UI SKELETON / MOCK"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    root->addWidget(heading);

    auto* body = new QHBoxLayout;
    auto* preview = new QFrame(this);
    preview->setObjectName(QStringLiteral("previewPlaceholder"));
    preview->setStyleSheet("QFrame#previewPlaceholder { background: #101721; border: 2px dashed #52657d; "
                           "border-radius: 10px; }");
    auto* preview_layout = new QVBoxLayout(preview);
    auto* preview_label = new QLabel(
        QStringLiteral("PREVIEW PLACEHOLDER\nNo V4L2 device is opened\nNo pixel buffer is attached"), preview);
    preview_label->setAlignment(Qt::AlignCenter);
    preview_label->setStyleSheet("color: #aebbd0; font-size: 16px;");
    preview_layout->addWidget(preview_label);
    body->addWidget(preview, 3);

    auto* side = new QVBoxLayout;
    front_status_ = new StatusBadge(QStringLiteral("Front"), this);
    rear_status_ = new StatusBadge(QStringLiteral("Rear"), this);
    recording_status_ = new StatusBadge(QStringLiteral("Record"), this);
    rtsp_status_ = new StatusBadge(QStringLiteral("RTSP"), this);
    source_value_ = new QLabel(QStringLiteral("Source: Front"), this);
    fps_value_ = new QLabel(QStringLiteral("FPS / infer: --"), this);
    side->addWidget(front_status_);
    side->addWidget(rear_status_);
    side->addWidget(recording_status_);
    side->addWidget(rtsp_status_);
    side->addSpacing(8);
    side->addWidget(source_value_);
    side->addWidget(fps_value_);
    side->addStretch();
    body->addLayout(side, 2);
    root->addLayout(body, 1);

    auto* controls = new QHBoxLayout;
    auto* front = actionButton(QStringLiteral("Front"), this);
    auto* rear = actionButton(QStringLiteral("Rear"), this);
    auto* snapshot = actionButton(QStringLiteral("Snapshot"), this);
    auto* record = actionButton(QStringLiteral("Record"), this);
    auto* rtsp = actionButton(QStringLiteral("RTSP"), this);
    controls->addWidget(front);
    controls->addWidget(rear);
    controls->addWidget(snapshot);
    controls->addWidget(record);
    controls->addWidget(rtsp);
    root->addLayout(controls);

    result_ = new QLabel(QStringLiteral("Ready: MOCK backend; controls return explicit RESULT"), this);
    result_->setMinimumHeight(28);
    result_->setWordWrap(true);
    root->addWidget(result_);

    connect(front, &QPushButton::clicked, this, &CameraPage::frontRequested);
    connect(rear, &QPushButton::clicked, this, &CameraPage::rearRequested);
    connect(snapshot, &QPushButton::clicked, this, &CameraPage::snapshotRequested);
    connect(record, &QPushButton::clicked, this, &CameraPage::recordingRequested);
    connect(rtsp, &QPushButton::clicked, this, &CameraPage::rtspRequested);
}

void CameraPage::setState(const UiState& state) {
    front_status_->setStatus(state.camera_front);
    rear_status_->setStatus(state.camera_rear);
    recording_status_->setStatus(state.recording);
    rtsp_status_->setStatus(state.rtsp);
    source_value_->setText(QStringLiteral("Source: %1 · MOCK")
                               .arg(QString::fromStdString(state.current_camera)));
    fps_value_->setText(QStringLiteral("FPS / infer: %1")
                            .arg(QString::fromStdString(state.inference_rate)));
}

void CameraPage::showResult(const UiResult& result) {
    result_->setText(QStringLiteral("RESULT #%1: %2")
                         .arg(static_cast<qulonglong>(result.request_id))
                         .arg(QString::fromStdString(result.message)));
    result_->setStyleSheet(result.succeeded() ? "color: #8dd9b0;" : "color: #ff9aa6;");
}

}  // namespace cockpit::ui
