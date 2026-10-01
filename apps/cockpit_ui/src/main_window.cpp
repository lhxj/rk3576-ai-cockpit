#include "cockpit_ui/main_window.h"

#include "pages/ai_page.h"
#include "pages/camera_page.h"
#include "pages/home_page.h"
#include "pages/media_page.h"
#include "pages/monitor_page.h"
#include "pages/settings_page.h"
#include "pages/vehicle_page.h"
#include "widgets/navigation_button.h"
#include "widgets/status_badge.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QStackedWidget>
#include <QThread>
#include <QVBoxLayout>

#include <utility>

namespace cockpit::ui {

MainWindow::MainWindow(std::unique_ptr<IUiBackend> backend, QWidget* parent)
    : QMainWindow(parent), backend_(std::move(backend)) {
    buildUi();
    connectActions();
    backend_->setStateCallback([this](UiState state) {
        if (QThread::currentThread() == thread()) {
            applyState(state);
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, state = std::move(state)] { applyState(state); },
            Qt::QueuedConnection);
    });
    backend_->setResultCallback([this](UiResult result) {
        if (QThread::currentThread() == thread()) {
            applyResult(result);
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, result = std::move(result)] { applyResult(result); },
            Qt::QueuedConnection);
    });
    if (!backend_->start()) {
        auto failed = backend_->currentState();
        failed.latest_result = "Backend failed to start";
        applyState(failed);
    }
    navigate(PageId::Home);
}

MainWindow::~MainWindow() {
    if (backend_) {
        backend_->setStateCallback({});
        backend_->setResultCallback({});
        backend_->stop();
    }
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("RK3576 AI Cockpit"));
    setMinimumSize(640, 360);
    resize(800, 480);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* top_bar = new QFrame(central);
    top_bar->setObjectName(QStringLiteral("topBar"));
    top_bar->setFixedHeight(54);
    auto* top_layout = new QHBoxLayout(top_bar);
    top_layout->setContentsMargins(14, 7, 14, 7);
    top_layout->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("RK3576 AI Cockpit"), top_bar);
    title->setStyleSheet("font-size: 19px; font-weight: 700;");
    backend_mode_ = new QLabel(QStringLiteral("STARTING"), top_bar);
    backend_mode_->setObjectName(QStringLiteral("backendMode"));
    backend_mode_->setStyleSheet("color: #f3c96a; font-weight: 700; padding: 4px 8px; "
                                  "border: 1px solid #8a6417; border-radius: 7px;");
    wifi_status_ = new StatusBadge(QStringLiteral("Wi-Fi"), top_bar);
    rtos_status_ = new StatusBadge(QStringLiteral("RTOS"), top_bar);
    top_layout->addWidget(title);
    top_layout->addWidget(backend_mode_);
    top_layout->addStretch();
    top_layout->addWidget(wifi_status_);
    top_layout->addWidget(rtos_status_);
    root->addWidget(top_bar);

    stack_ = new QStackedWidget(central);
    home_page_ = new HomePage(stack_);
    camera_page_ = new CameraPage(stack_);
    media_page_ = new MediaPage(stack_);
    vehicle_page_ = new VehiclePage(stack_);
    ai_page_ = new AiPage(stack_);
    monitor_page_ = new MonitorPage(stack_);
    settings_page_ = new SettingsPage(stack_);
    stack_->addWidget(home_page_);
    stack_->addWidget(camera_page_);
    stack_->addWidget(media_page_);
    stack_->addWidget(vehicle_page_);
    stack_->addWidget(ai_page_);
    stack_->addWidget(monitor_page_);
    stack_->addWidget(settings_page_);
    root->addWidget(stack_, 1);

    auto* navigation = new QFrame(central);
    navigation->setObjectName(QStringLiteral("navigationBar"));
    auto* navigation_layout = new QHBoxLayout(navigation);
    navigation_layout->setContentsMargins(7, 5, 7, 5);
    navigation_layout->setSpacing(5);
    for (const auto page : kPageOrder) {
        const auto name = pageName(page);
        auto* button = new NavigationButton(
            QString::fromUtf8(name.data(), static_cast<int>(name.size())), navigation);
        button->setObjectName(QStringLiteral("nav_%1").arg(
            QString::fromUtf8(name.data(), static_cast<int>(name.size())).toLower()));
        connect(button, &QPushButton::clicked, this, [this, page] { navigate(page); });
        navigation_layout->addWidget(button);
        navigation_buttons_.push_back(button);
    }
    root->addWidget(navigation);
    setCentralWidget(central);

    setStyleSheet(
        "QMainWindow, QWidget { background: #111923; color: #edf3fa; font-size: 14px; }"
        "QFrame#topBar, QFrame#navigationBar { background: #182433; }"
        "QPushButton { color: #edf3fa; background: #2a3b50; border: 1px solid #465c75; "
        "border-radius: 8px; padding: 7px; font-weight: 600; }"
        "QPushButton:pressed { background: #3a526e; }"
        "QPushButton:checked { background: #176f85; border-color: #62c6dc; }"
        "QPushButton:disabled { color: #8491a3; background: #222d3a; }"
        "QGroupBox { border: 1px solid #3f5168; border-radius: 9px; margin-top: 8px; "
        "padding-top: 8px; font-weight: 700; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
}

void MainWindow::connectActions() {
    connect(home_page_, &HomePage::navigateRequested, this, &MainWindow::navigate);

    connect(camera_page_, &CameraPage::frontRequested, this, [this] {
        camera_page_->showResult(dispatch(UiCommand::SwitchCamera, "front"));
    });
    connect(camera_page_, &CameraPage::rearRequested, this, [this] {
        camera_page_->showResult(dispatch(UiCommand::SwitchCamera, "rear"));
    });
    connect(camera_page_, &CameraPage::snapshotRequested, this, [this] {
        camera_page_->showResult(dispatch(UiCommand::Snapshot));
    });
    connect(camera_page_, &CameraPage::recordingRequested, this, [this](bool start) {
        camera_page_->showResult(dispatch(start ? UiCommand::RecordingStart
                                                : UiCommand::RecordingStop));
    });
    connect(camera_page_, &CameraPage::rtspRequested, this, [this](bool start) {
        camera_page_->showResult(dispatch(start ? UiCommand::RtspStart : UiCommand::RtspStop));
    });

    connect(media_page_, &MediaPage::playRequested, this, [this] {
        media_page_->showResult(dispatch(UiCommand::MediaPlay));
    });
    connect(media_page_, &MediaPage::pauseRequested, this, [this] {
        media_page_->showResult(dispatch(UiCommand::MediaPause));
    });
    connect(media_page_, &MediaPage::previousRequested, this, [this] {
        media_page_->showResult(dispatch(UiCommand::MediaPrevious));
    });
    connect(media_page_, &MediaPage::nextRequested, this, [this] {
        media_page_->showResult(dispatch(UiCommand::MediaNext));
    });
    connect(media_page_, &MediaPage::stopRequested, this, [this] {
        media_page_->showResult(dispatch(UiCommand::MediaStop));
    });

    connect(vehicle_page_, &VehiclePage::ledRequested, this, [this](bool enabled) {
        vehicle_page_->showResult(dispatch(UiCommand::LedSet, {}, enabled));
    });
    connect(vehicle_page_, &VehiclePage::buzzerRequested, this, [this](bool enabled) {
        vehicle_page_->showResult(dispatch(UiCommand::BuzzerSet, {}, enabled));
    });

    connect(ai_page_, &AiPage::voiceSessionRequested, this, [this](bool start) {
        ai_page_->showResult(dispatch(start ? UiCommand::VoiceSessionStart
                                            : UiCommand::VoiceSessionCancel));
    });
}

void MainWindow::navigate(PageId page) {
    const auto index = pageIndex(page);
    if (index >= navigation_buttons_.size()) {
        return;
    }
    stack_->setCurrentIndex(static_cast<int>(index));
    for (std::size_t button_index = 0; button_index < navigation_buttons_.size();
         ++button_index) {
        navigation_buttons_.at(button_index)->setChecked(button_index == index);
    }
}

void MainWindow::applyState(const UiState& state) {
    backend_mode_->setText(QString::fromStdString(state.backend_mode));
    wifi_status_->setStatus(state.wifi);
    rtos_status_->setStatus(state.rtos);
    home_page_->setState(state);
    camera_page_->setState(state);
    media_page_->setState(state);
    vehicle_page_->setState(state);
    ai_page_->setState(state);
    monitor_page_->setState(state);
    settings_page_->setState(state);
}

void MainWindow::applyResult(const UiResult& result) {
    switch (result.command) {
    case UiCommand::SwitchCamera:
    case UiCommand::Snapshot:
    case UiCommand::RecordingStart:
    case UiCommand::RecordingStop:
    case UiCommand::RtspStart:
    case UiCommand::RtspStop:
        camera_page_->showResult(result);
        break;
    case UiCommand::MediaPlay:
    case UiCommand::MediaPause:
    case UiCommand::MediaPrevious:
    case UiCommand::MediaNext:
    case UiCommand::MediaStop:
        media_page_->showResult(result);
        break;
    case UiCommand::VoiceSessionStart:
    case UiCommand::VoiceSessionCancel:
        ai_page_->showResult(result);
        break;
    case UiCommand::LedSet:
    case UiCommand::BuzzerSet:
        vehicle_page_->showResult(result);
        break;
    }
}

UiResult MainWindow::dispatch(UiCommand command, std::string argument, bool enabled) {
    return backend_->submit({command, std::move(argument), enabled});
}

}  // namespace cockpit::ui
