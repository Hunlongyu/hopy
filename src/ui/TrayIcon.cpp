#include "ui/TrayIcon.h"
#include "util/I18n.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>

namespace hopy {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    tray_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/logo.ico")), this);

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

} // namespace hopy
