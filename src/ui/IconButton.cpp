#include "ui/IconButton.h"
#include "util/Icons.h"
#include <QApplication>
#include <QEvent>
#include <QIcon>

namespace hopy {

IconButton::IconButton(const QString& svgName, const QString& tip, bool checkable, QWidget* parent)
    : QToolButton(parent), svg_(svgName) {
    setToolTip(tip);
    setCheckable(checkable);
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setIconSize(QSize(16, 16));
    rebuildIcon();
}

void IconButton::changeEvent(QEvent* ev) {
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange)
        rebuildIcon();
    QToolButton::changeEvent(ev);
}

void IconButton::rebuildIcon() {
    const QPalette pal = QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightnessF() < 0.5;
    const QColor off   = dark ? QColor(0xd8, 0xd8, 0xda) : QColor(0x45, 0x48, 0x4d); // idle
    const QColor hover = dark ? QColor(0xff, 0xff, 0xff) : QColor(0x1a, 0x1a, 0x1a); // hover
    const QColor on(0xff, 0xff, 0xff);                                              // checked (blue bg)

    QIcon ic;
    ic.addPixmap(icons::svgPixmap(svg_, off, 16),   QIcon::Normal, QIcon::Off);
    ic.addPixmap(icons::svgPixmap(svg_, hover, 16), QIcon::Active, QIcon::Off);
    if (isCheckable()) {
        ic.addPixmap(icons::svgPixmap(svg_, on, 16), QIcon::Normal, QIcon::On);
        ic.addPixmap(icons::svgPixmap(svg_, on, 16), QIcon::Active, QIcon::On);
    }
    setIcon(ic);
}

} // namespace hopy
