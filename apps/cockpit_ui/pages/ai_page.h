#pragma once

#include "cockpit_ui/ui_backend.h"

#include <QWidget>

class QLabel;
class QPushButton;

namespace cockpit::ui {

class StatusBadge;

class AiPage final : public QWidget {
    Q_OBJECT

public:
    explicit AiPage(QWidget* parent = nullptr);
    void setState(const UiState& state);
    void showResult(const UiResult& result);

signals:
    void voiceSessionRequested(bool start);

private:
    StatusBadge* vision_status_{nullptr};
    StatusBadge* voice_status_{nullptr};
    StatusBadge* llm_status_{nullptr};
    QLabel* vision_details_{nullptr};
    QLabel* voice_details_{nullptr};
    QLabel* result_{nullptr};
    QPushButton* session_button_{nullptr};
    bool session_active_{false};
};

}  // namespace cockpit::ui
