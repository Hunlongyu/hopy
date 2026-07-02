#include "ui/ScreenPlacement.h"
#include <algorithm>
#include <limits>

namespace hopy {

QRect placeWindow(const QPoint& cursor, const QList<QRect>& screens, const QSize& win) {
    if (screens.isEmpty()) return QRect(QPoint(0, 0), win);

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

    int x = chosen->center().x() - win.width() / 2;
    int y = chosen->center().y() - win.height() / 2;

    if (win.width() >= chosen->width())  x = chosen->left();
    else x = std::clamp(x, chosen->left(), chosen->right() - win.width() + 1);
    if (win.height() >= chosen->height()) y = chosen->top();
    else y = std::clamp(y, chosen->top(), chosen->bottom() - win.height() + 1);

    return QRect(QPoint(x, y), win);
}

} // namespace hopy
