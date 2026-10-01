#pragma once

#include "cockpit_ui/ui_backend.h"

#include <QWidget>

class QLabel;

namespace cockpit::ui {

class StatusBadge;

class MediaPage final : public QWidget {
    Q_OBJECT

public:
    explicit MediaPage(QWidget* parent = nullptr);
    void setState(const UiState& state);
    void showResult(const UiResult& result);

signals:
    void playRequested();
    void pauseRequested();
    void previousRequested();
    void nextRequested();
    void stopRequested();

private:
    StatusBadge* audio_status_{nullptr};
    QLabel* result_{nullptr};
};

}  // namespace cockpit::ui
