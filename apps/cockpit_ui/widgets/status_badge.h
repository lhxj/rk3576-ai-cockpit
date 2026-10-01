#pragma once

#include "cockpit_ui/ui_state.h"

#include <QLabel>
#include <QSizePolicy>

namespace cockpit::ui {

class StatusBadge final : public QLabel {
public:
    explicit StatusBadge(const QString& name, QWidget* parent = nullptr);

    void setStatus(const ServiceStatus& status);

private:
    QString name_;
};

}  // namespace cockpit::ui
