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

// The foreground window's text caret in Qt logical global coordinates, ready to
// feed to placeWindow(). Reads the physical caret (platform::queryCaret) and maps
// it through the matching QScreen, handling mixed per-monitor scaling. Returns
// false when no usable caret is found (caller falls back to the mouse).
bool caretAnchorLogical(QPoint& out);
}

