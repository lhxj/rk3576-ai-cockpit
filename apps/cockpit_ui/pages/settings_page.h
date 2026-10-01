#pragma once

#include "cockpit_ui/ui_state.h"

#include <QWidget>

class QLabel;

namespace cockpit::ui {

class SettingsPage final : public QWidget {
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    void setState(const UiState& state);

private:
    QLabel* state_note_{nullptr};
};

}  // namespace cockpit::ui
