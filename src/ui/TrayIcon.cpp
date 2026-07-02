#include "ui/TrayIcon.h"
#include "util/Icons.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>

namespace hopy {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    QIcon icon = icons::glyphIcon(0xE8C8 /* Copy */, QColor(0xf0, 0xf0, 0xf0), 32);
    tray_ = new QSystemTrayIcon(icon, this);

    auto* menu = new QMenu();
    QAction* showAct = menu->addAction(QStringLiteral("显示 (Show)"));
    QAction* setAct  = menu->addAction(QStringLiteral("设置 (Settings)"));
    menu->addSeparator();
    QAction* quitAct = menu->addAction(QStringLiteral("退出 (Quit)"));
    connect(showAct, &QAction::triggered, this, &TrayIcon::showRequested);
    connect(setAct,  &QAction::triggered, this, &TrayIcon::settingsRequested);
    connect(quitAct, &QAction::triggered, this, &TrayIcon::quitRequested);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });

    tray_->setContextMenu(menu);
    tray_->setToolTip(QStringLiteral("hopy"));
    tray_->show();
}

} // namespace hopy
