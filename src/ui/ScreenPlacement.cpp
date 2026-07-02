#include "ui/ScreenPlacement.h"
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

} // namespace hopy
