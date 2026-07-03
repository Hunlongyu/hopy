#pragma once
#include <QRect>
#include <QString>
#include <QList>

namespace hopy::platform {

// A monitor's rectangle in NATIVE / physical screen pixels, plus its GDI device
// name — used to convert a physical caret position to Qt logical coordinates
// under mixed per-monitor scaling.
struct MonitorInfo {
    QRect rect;
    QString device;
};
QList<MonitorInfo> physicalMonitors();

} // namespace hopy::platform
