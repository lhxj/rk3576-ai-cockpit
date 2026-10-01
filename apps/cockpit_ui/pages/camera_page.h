#pragma once

#include "cockpit_ui/ui_backend.h"
#include "cockpit_ui/preview_frame_metadata.h"

#include <QImage>
#include <QWidget>

class QLabel;
class QPushButton;
class QResizeEvent;

namespace cockpit::ui {

class StatusBadge;

class CameraPage final : public QWidget {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);
    void setState(const UiState& state);
    void showResult(const UiResult& result);
    void setPreviewFrame(QImage image, const PreviewFrameMetadata& metadata,
                         double preview_fps, std::uint64_t mailbox_drops,
                         std::uint64_t ui_drops);

signals:
    void frontRequested();
    void rearRequested();
    void snapshotRequested();
    void recordingRequested(bool start);
    void rtspRequested(bool start);

private:
    void resizeEvent(QResizeEvent* event) override;
    void refreshPreviewPixmap();

    StatusBadge* front_status_{nullptr};
    StatusBadge* rear_status_{nullptr};
    StatusBadge* preview_status_{nullptr};
    StatusBadge* recording_status_{nullptr};
    StatusBadge* rtsp_status_{nullptr};
    QLabel* source_value_{nullptr};
    QLabel* fps_value_{nullptr};
    QLabel* preview_image_{nullptr};
    QLabel* result_{nullptr};
    QPushButton* recording_button_{nullptr};
    QPushButton* rtsp_button_{nullptr};
    bool recording_active_{false};
    bool rtsp_active_{false};
    QImage last_preview_image_;
};

}  // namespace cockpit::ui
