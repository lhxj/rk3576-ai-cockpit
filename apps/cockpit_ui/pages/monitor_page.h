#pragma once

#include "cockpit_ui/ui_state.h"

#include <QWidget>

#include <vector>

class QLabel;

namespace cockpit::ui {

class MonitorPage final : public QWidget {
public:
    explicit MonitorPage(QWidget* parent = nullptr);
    void setState(const UiState& state);

private:
    QLabel* cpu_{nullptr};
    QLabel* memory_{nullptr};
    QLabel* temperature_{nullptr};
    QLabel* disk_{nullptr};
    QLabel* service_summary_{nullptr};
};

}  // namespace cockpit::ui
