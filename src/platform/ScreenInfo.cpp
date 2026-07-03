#include "platform/ScreenInfo.h"

#if defined(Q_OS_WIN)
#include <windows.h>

namespace hopy::platform {
QList<MonitorInfo> physicalMonitors() {
    QList<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR h, HDC, LPRECT, LPARAM p) -> BOOL {
            auto* out = reinterpret_cast<QList<MonitorInfo>*>(p);
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(h, &mi)) {
                const RECT& r = mi.rcMonitor;
                out->append({ QRect(r.left, r.top, r.right - r.left, r.bottom - r.top),
                              QString::fromWCharArray(mi.szDevice) });
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&list));
    return list;
}
} // namespace hopy::platform

#else

namespace hopy::platform {
QList<MonitorInfo> physicalMonitors() { return {}; }
} // namespace hopy::platform

#endif
