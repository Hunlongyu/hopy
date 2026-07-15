#pragma once
#include <QObject>
#include <QIcon>
class QSystemTrayIcon;
class QAction;
class QMenu;
namespace hopy {
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void retranslate();             // re-label the menu after a language change
    void applyTheme();              // re-tint the menu icons for the current palette
    void setUpdateBadge(bool on);   // overlay a red dot on the tray icon when an update is pending
    void setPaused(bool on);        // reflect the pause state on the menu item (no signal)
    void setAutostart(bool on);     // reflect the autostart state on the menu item (no signal)
signals:
    void showRequested();
    void settingsRequested();
    void checkUpdateRequested();
    void pauseToggled(bool paused);
    void autostartToggled(bool enabled);
    void quitRequested();
private:
    void rebuildIcons();            // (re)assign palette-tinted icons to every action
    void syncToggle(QAction* act, const QString& iconName, const char* baseText);   // reflect a checkable row's on/off state (accent icon + trailing ✓)
    void applyToolTip();            // hover tooltip: name · description · version (3 lines)
    QSystemTrayIcon* tray_ = nullptr;
    QMenu* menu_ = nullptr;
    QAction* showAct_ = nullptr;
    QAction* checkAct_ = nullptr;
    QAction* setAct_ = nullptr;
    QAction* pauseAct_ = nullptr;
    QAction* autostartAct_ = nullptr;
    QAction* quitAct_ = nullptr;
    QIcon baseIcon_;                // the un-badged logo, restored when the badge clears
};
}
