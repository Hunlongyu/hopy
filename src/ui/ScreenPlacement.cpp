#include "ui/ScreenPlacement.h"
#include "platform/CaretProbe.h"
#include "platform/ScreenInfo.h"
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>
#include <limits>

namespace hopy {

QRect placeWindow(const QPoint& cursor, const QList<QRect>& screens, const QSize& win,
                  WindowPlacement mode) {
    if (screens.isEmpty()) return QRect(QPoint(0, 0), win);

    // Pick the screen under the cursor, else the one whose centre is nearest.
    const QRect* chosen = nullptr;
    for (const QRect& g : screens) {
        if (g.contains(cursor)) { chosen = &g; break; }
    }
    if (!chosen) {
        qint64 best = std::numeric_limits<qint64>::max();
        for (const QRect& g : screens) {
            const QPoint c = g.center();
            const qint64 dx = c.x() - cursor.x();
            const qint64 dy = c.y() - cursor.y();
            const qint64 d = dx * dx + dy * dy;
            if (d < best) { best = d; chosen = &g; }
        }
    }

    int x, y;
    if (mode == WindowPlacement::Cursor) {
        // Open down-right of the cursor like Win+V; flip to the other side when
        // there isn't room, so the panel never spills off the screen edge.
        x = cursor.x();
        y = cursor.y();
        if (x + win.width()  > chosen->right()  + 1) x = cursor.x() - win.width();
        if (y + win.height() > chosen->bottom() + 1) y = cursor.y() - win.height();
    } else {
        x = chosen->center().x() - win.width() / 2;
        y = chosen->center().y() - win.height() / 2;
    }

    // Clamp fully inside the chosen screen (also handles window-larger-than-screen).
    if (win.width() >= chosen->width())   x = chosen->left();
    else x = std::clamp(x, chosen->left(), chosen->right() - win.width() + 1);
    if (win.height() >= chosen->height()) y = chosen->top();
    else y = std::clamp(y, chosen->top(), chosen->bottom() - win.height() + 1);

    return QRect(QPoint(x, y), win);
}

bool caretAnchorLogical(QPoint& out) {
    platform::CaretInfo ci;
    if (!platform::queryCaret(ci)) return false;
    // Anchor at the caret's bottom-left so the panel drops just below the line,
    // but locate the monitor by the caret CENTRE — the bottom edge of a caret on
    // the screen's last row can fall just outside the monitor rect.
    const QPoint physAnchor(ci.caret.left(), ci.caret.bottom());
    const QPoint physCentre((ci.caret.left() + ci.caret.right()) / 2,
                            (ci.caret.top() + ci.caret.bottom()) / 2);

    // Which PHYSICAL monitor holds the caret (Win32 pixels)?
    const QList<platform::MonitorInfo> monitors = platform::physicalMonitors();
    int mi = -1;
    for (int i = 0; i < monitors.size(); ++i)
        if (monitors[i].rect.contains(physCentre)) { mi = i; break; }
    if (mi < 0) return false;
    const QRect physMon = monitors[mi].rect;

    // Map that physical monitor to its QScreen — by GDI device name, and if that
    // fails (names don't always line up) by spatial rank. Relying on names alone
    // is what sent the window to the wrong monitor before.
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen* target = nullptr;
    for (QScreen* s : screens)
        if (s->name() == monitors[mi].device) { target = s; break; }
    if (!target && !screens.isEmpty()) {
        auto before = [](QPoint a, QPoint b) {
            return a.y() < b.y() || (a.y() == b.y() && a.x() < b.x());
        };
        int rank = 0;
        for (int i = 0; i < monitors.size(); ++i)
            if (i != mi && before(monitors[i].rect.topLeft(), physMon.topLeft())) ++rank;
        QList<QScreen*> ss = screens;
        std::sort(ss.begin(), ss.end(), [&](QScreen* a, QScreen* b) {
            return before(a->geometry().topLeft(), b->geometry().topLeft());
        });
        if (rank < ss.size()) target = ss[rank];
    }
    if (!target) return false;

    // Fractional position within the physical monitor → same fraction of the
    // logical monitor. DPI-agnostic; equivalent to (physOffset / screenScale).
    const QRect logi = target->geometry();
    const double fx = physMon.width()  ? double(physAnchor.x() - physMon.x()) / physMon.width()  : 0.0;
    const double fy = physMon.height() ? double(physAnchor.y() - physMon.y()) / physMon.height() : 0.0;
    out = QPoint(logi.x() + qRound(fx * logi.width()),
                 logi.y() + qRound(fy * logi.height()));
    return true;
}

} // namespace hopy
