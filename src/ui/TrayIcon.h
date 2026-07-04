#pragma once
#include <QObject>
class QSystemTrayIcon;
class QAction;
namespace hopy {
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void retranslate();   // re-label the menu after a language change
signals:
    void showRequested();
    void settingsRequested();
    void quitRequested();
private:
    QSystemTrayIcon* tray_ = nullptr;
    QAction* showAct_ = nullptr;
    QAction* setAct_ = nullptr;
    QAction* quitAct_ = nullptr;
};
}
