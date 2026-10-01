#include "pages/media_page.h"

#include "widgets/status_badge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace cockpit::ui {
namespace {

QPushButton* control(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(52);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

}  // namespace

MediaPage::MediaPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 12, 18, 12);
    root->setSpacing(12);

    auto* header = new QHBoxLayout;
    auto* heading = new QLabel(QStringLiteral("Media · MOCK CONTROLS"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    audio_status_ = new StatusBadge(QStringLiteral("Audio"), this);
    header->addWidget(heading);
    header->addStretch();
    header->addWidget(audio_status_);
    root->addLayout(header);

    auto* now_playing = new QLabel(QStringLiteral("Now Playing\nNo media loaded · media_srv not connected"), this);
    now_playing->setAlignment(Qt::AlignCenter);
    now_playing->setMinimumHeight(92);
    now_playing->setStyleSheet("background: #1c2939; border-radius: 10px; font-size: 17px;");
    root->addWidget(now_playing);

    auto* progress = new QProgressBar(this);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setFormat(QStringLiteral("Progress placeholder"));
    progress->setEnabled(false);
    root->addWidget(progress);

    auto* controls = new QHBoxLayout;
    auto* previous = control(QStringLiteral("Previous"), this);
    auto* play = control(QStringLiteral("Play"), this);
    auto* pause = control(QStringLiteral("Pause"), this);
    auto* next = control(QStringLiteral("Next"), this);
    auto* stop = control(QStringLiteral("Stop"), this);
    previous->setObjectName(QStringLiteral("media_previous"));
    play->setObjectName(QStringLiteral("media_play"));
    pause->setObjectName(QStringLiteral("media_pause"));
    next->setObjectName(QStringLiteral("media_next"));
    stop->setObjectName(QStringLiteral("media_stop"));
    controls->addWidget(previous);
    controls->addWidget(play);
    controls->addWidget(pause);
    controls->addWidget(next);
    controls->addWidget(stop);
    root->addLayout(controls);

    auto* volume_row = new QHBoxLayout;
    volume_row->addWidget(new QLabel(QStringLiteral("Volume placeholder"), this));
    auto* volume = new QSlider(Qt::Horizontal, this);
    volume->setValue(50);
    volume->setEnabled(false);
    volume_row->addWidget(volume, 1);
    root->addLayout(volume_row);

    result_ = new QLabel(QStringLiteral("Media list placeholder · no filesystem scan"), this);
    result_->setObjectName(QStringLiteral("media_result"));
    result_->setWordWrap(true);
    root->addWidget(result_);

    connect(play, &QPushButton::clicked, this, &MediaPage::playRequested);
    connect(pause, &QPushButton::clicked, this, &MediaPage::pauseRequested);
    connect(previous, &QPushButton::clicked, this, &MediaPage::previousRequested);
    connect(next, &QPushButton::clicked, this, &MediaPage::nextRequested);
    connect(stop, &QPushButton::clicked, this, &MediaPage::stopRequested);
}

void MediaPage::setState(const UiState& state) {
    audio_status_->setStatus(state.media_service);
}

void MediaPage::showResult(const UiResult& result) {
    result_->setText(QStringLiteral("RESULT #%1: %2")
                         .arg(static_cast<qulonglong>(result.request_id))
                         .arg(QString::fromStdString(result.message)));
    result_->setStyleSheet(result.succeeded() ? "color: #8dd9b0;" : "color: #ff9aa6;");
}

}  // namespace cockpit::ui
