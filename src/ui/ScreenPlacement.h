#pragma once
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QList>

namespace hopy {
QRect placeWindow(const QPoint& cursorGlobal, const QList<QRect>& screens, const QSize& windowSize);
}
