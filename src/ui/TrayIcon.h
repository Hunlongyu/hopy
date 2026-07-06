#pragma once
#include <QObject>
#include <QIcon>
class QSystemTrayIcon;
class QAction;
namespace hopy {
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void retranslate();          // re-label the menu after a language change
    void setUpdateBadge(bool on);   // overlay a red dot on the tray icon when an update is pending
signals:
    void showRequested();
    void settingsRequested();
    void quitRequested();
private:
    QSystemTrayIcon* tray_ = nullptr;
    QAction* showAct_ = nullptr;
    QAction* setAct_ = nullptr;
    QAction* quitAct_ = nullptr;
    QIcon baseIcon_;             // the un-badged logo, restored when the badge clears
};
}
