#include "widgets/navigation_button.h"

namespace cockpit::ui {

NavigationButton::NavigationButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {
    setCheckable(true);
    setMinimumHeight(48);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
}

}  // namespace cockpit::ui

