#pragma once
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QList>

namespace hopy {

enum class WindowPlacement {
    Center,   // centered on the screen under the cursor (legacy behaviour)
    Cursor,   // anchored at the cursor, flipping/clamping to stay on-screen
};

QRect placeWindow(const QPoint& cursorGlobal, const QList<QRect>& screens,
                  const QSize& windowSize, WindowPlacement mode = WindowPlacement::Center);
}
