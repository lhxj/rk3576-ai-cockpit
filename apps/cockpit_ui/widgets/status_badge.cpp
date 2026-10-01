#include "widgets/status_badge.h"

#include <QString>

namespace cockpit::ui {
namespace {

QString backgroundFor(AvailabilityState state) {
    switch (state) {
    case AvailabilityState::Online:
        return "#1f7a4d";
    case AvailabilityState::Degraded:
    case AvailabilityState::Starting:
        return "#8a6417";
    case AvailabilityState::Simulated:
        return "#315f91";
    case AvailabilityState::Offline:
    case AvailabilityState::NotReady:
    case AvailabilityState::Unavailable:
        return "#4e5968";
    case AvailabilityState::Error:
    case AvailabilityState::Timeout:
        return "#9c3541";
    case AvailabilityState::Unknown:
        return "#5a526e";
    }
    return "#4e5968";
}

}  // namespace

StatusBadge::StatusBadge(const QString& name, QWidget* parent)
    : QLabel(parent), name_(name) {
    setMinimumHeight(32);
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void StatusBadge::setStatus(const ServiceStatus& status) {
    const auto state = QString::fromUtf8(toString(status.state).data(),
                                         static_cast<int>(toString(status.state).size()));
    const auto source = QString::fromUtf8(toString(status.source).data(),
                                          static_cast<int>(toString(status.source).size()));
    setText(QStringLiteral("%1  %2 · %3").arg(name_, state, source));
    setToolTip(QString::fromStdString(status.detail));
    setStyleSheet(QStringLiteral("QLabel { color: white; background: %1; border-radius: 8px; "
                                 "padding: 5px 10px; font-weight: 600; }")
                      .arg(backgroundFor(status.state)));
}

}  // namespace cockpit::ui
