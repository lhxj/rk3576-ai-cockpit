#pragma once

#include <QPushButton>
#include <QSizePolicy>

namespace cockpit::ui {

class NavigationButton final : public QPushButton {
public:
    explicit NavigationButton(const QString& text, QWidget* parent = nullptr);
};

}  // namespace cockpit::ui
