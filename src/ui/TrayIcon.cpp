#include "ui/TrayIcon.h"
#include "util/I18n.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>

namespace hopy {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    tray_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/logo.ico")), this);

    auto* menu = new QMenu();
    QAction* showAct = menu->addAction(T("Show"));
    QAction* setAct  = menu->addAction(T("Settings"));
    menu->addSeparator();
    QAction* quitAct = menu->addAction(T("Quit"));
    connect(showAct, &QAction::triggered, this, &TrayIcon::showRequested);
    connect(setAct,  &QAction::triggered, this, &TrayIcon::settingsRequested);
    connect(quitAct, &QAction::triggered, this, &TrayIcon::quitRequested);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });

    tray_->setContextMenu(menu);
    tray_->setToolTip(QStringLiteral("Hopy"));
    tray_->show();
}

} // namespace hopy
