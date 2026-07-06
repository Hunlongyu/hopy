#include "ui/TrayIcon.h"
#include "util/I18n.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QRect>

namespace hopy {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    baseIcon_ = QIcon(QStringLiteral(":/logo.ico"));
    tray_ = new QSystemTrayIcon(baseIcon_, this);

    auto* menu = new QMenu();
    showAct_ = menu->addAction(T("Show"));
    setAct_  = menu->addAction(T("Settings"));
    menu->addSeparator();
    quitAct_ = menu->addAction(T("Quit"));
    connect(showAct_, &QAction::triggered, this, &TrayIcon::showRequested);
    connect(setAct_,  &QAction::triggered, this, &TrayIcon::settingsRequested);
    connect(quitAct_, &QAction::triggered, this, &TrayIcon::quitRequested);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });

    tray_->setContextMenu(menu);
    tray_->setToolTip(QStringLiteral("Hopy"));
    tray_->show();
}

void TrayIcon::retranslate() {
    showAct_->setText(T("Show"));
    setAct_->setText(T("Settings"));
    quitAct_->setText(T("Quit"));
}

void TrayIcon::setUpdateBadge(bool on) {
    if (!on) { tray_->setIcon(baseIcon_); return; }
    const int s = 64;
    QPixmap pm = baseIcon_.pixmap(s, s);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const int d = 22;                                    // red dot diameter
    const QRect dot(s - d - 2, s - d - 2, d, d);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0xff, 0xff));                // white ring for contrast on any tray
    p.drawEllipse(dot.adjusted(-3, -3, 3, 3));
    p.setBrush(QColor(0xe5, 0x53, 0x4b));                // red badge
    p.drawEllipse(dot);
    p.end();
    tray_->setIcon(QIcon(pm));
}

} // namespace hopy
