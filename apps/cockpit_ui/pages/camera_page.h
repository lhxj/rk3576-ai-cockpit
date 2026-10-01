#pragma once

#include "cockpit_ui/ui_backend.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace cockpit::ui {

class StatusBadge;

class CameraPage final : public QWidget {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);
    void setState(const UiState& state);
    void showResult(const UiResult& result);

signals:
    void frontRequested();
    void rearRequested();
    void snapshotRequested();
    void recordingRequested(bool start);
    void rtspRequested(bool start);

private:
    StatusBadge* front_status_{nullptr};
    StatusBadge* rear_status_{nullptr};
    StatusBadge* recording_status_{nullptr};
    StatusBadge* rtsp_status_{nullptr};
    QLabel* source_value_{nullptr};
    QLabel* fps_value_{nullptr};
    QLabel* result_{nullptr};
    QPushButton* recording_button_{nullptr};
    QPushButton* rtsp_button_{nullptr};
    bool recording_active_{false};
    bool rtsp_active_{false};
};

}  // namespace cockpit::ui
