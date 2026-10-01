#include "pages/home_page.h"

#include "widgets/status_badge.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace cockpit::ui {
namespace {

QPushButton* makeCard(const QString& title, const QString& description, QWidget* parent) {
    auto* button = new QPushButton(QStringLiteral("%1\n%2").arg(title, description), parent);
    button->setMinimumHeight(82);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button->setStyleSheet(
        "QPushButton { text-align: left; padding: 12px; font-size: 16px; font-weight: 600; "
        "background: #263447; border: 1px solid #3f5168; border-radius: 10px; }"
        "QPushButton:pressed { background: #354a63; }");
    return button;
}

}  // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 10);
    root->setSpacing(10);

    auto* heading = new QLabel(QStringLiteral("Cockpit MVP · MOCK / DEMO"), this);
    heading->setStyleSheet("font-size: 20px; font-weight: 700;");
    root->addWidget(heading);

    auto* badges = new QGridLayout;
    badges->setHorizontalSpacing(8);
    badges->setVerticalSpacing(6);
    camera_status_ = new StatusBadge(QStringLiteral("Front Cam"), this);
    audio_status_ = new StatusBadge(QStringLiteral("Audio"), this);
    ai_status_ = new StatusBadge(QStringLiteral("AI"), this);
    vehicle_status_ = new StatusBadge(QStringLiteral("RTOS"), this);
    badges->addWidget(camera_status_, 0, 0);
    badges->addWidget(audio_status_, 0, 1);
    badges->addWidget(ai_status_, 1, 0);
    badges->addWidget(vehicle_status_, 1, 1);
    root->addLayout(badges);

    auto* cards = new QGridLayout;
    cards->setSpacing(10);
    auto* camera = makeCard(QStringLiteral("Camera"), QStringLiteral("Preview and controls"), this);
    auto* media = makeCard(QStringLiteral("Media"), QStringLiteral("Music and video controls"), this);
    auto* vehicle = makeCard(QStringLiteral("Vehicle"), QStringLiteral("RTOS and sensor state"), this);
    auto* ai = makeCard(QStringLiteral("AI"), QStringLiteral("Vision and voice status"), this);
    auto* monitor = makeCard(QStringLiteral("Monitor"), QStringLiteral("System health placeholders"), this);
    auto* settings = makeCard(QStringLiteral("Settings"), QStringLiteral("UI-only preferences"), this);
    cards->addWidget(camera, 0, 0);
    cards->addWidget(media, 0, 1);
    cards->addWidget(vehicle, 0, 2);
    cards->addWidget(ai, 1, 0);
    cards->addWidget(monitor, 1, 1);
    cards->addWidget(settings, 1, 2);
    root->addLayout(cards, 1);

    connect(camera, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Camera); });
    connect(media, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Media); });
    connect(vehicle, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Vehicle); });
    connect(ai, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Ai); });
    connect(monitor, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Monitor); });
    connect(settings, &QPushButton::clicked, this, [this] { emit navigateRequested(PageId::Settings); });
}

void HomePage::setState(const UiState& state) {
    camera_status_->setStatus(state.camera_front);
    audio_status_->setStatus(state.audio);
    ai_status_->setStatus(state.vision);
    vehicle_status_->setStatus(state.rtos);
}

}  // namespace cockpit::ui
