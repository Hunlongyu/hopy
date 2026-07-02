#pragma once
#include <QObject>
class QSystemTrayIcon;
namespace hopy {
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
signals:
    void showRequested();
    void settingsRequested();
    void quitRequested();
private:
    QSystemTrayIcon* tray_ = nullptr;
};
}
