#pragma once
#include <QObject>

class QWidget;

namespace hopy::platform {

// While started, a non-activating window can still be driven by the keyboard:
// a low-level keyboard hook translates key presses into QKeyEvents posted to a
// target widget (and swallows them so the focused editor never sees them). A
// foreground WinEvent hook reports when the user switches away, so the window
// can auto-hide. Windows-only; a no-op on other platforms.
class InputHook : public QObject {
    Q_OBJECT
public:
    explicit InputHook(QObject* parent = nullptr);
    ~InputHook() override;

    void start(QWidget* keyTarget);   // install hooks, route keys to keyTarget
    void stop();                      // uninstall

    // While active, the mouse side buttons (M4/M5) are captured and reported via
    // sideScroll() instead of reaching Qt — so they drive the preview scroll and can
    // never be mis-read as clicks/double-clicks. Enabled only while a preview is shown.
    void setSideScrollActive(bool on);

signals:
    void dismissRequested();          // user clicked away / switched windows → hide
    void sideScroll(int dir, bool pressed);   // M4 (dir +1, down) / M5 (dir -1, up) pressed or released
};

} // namespace hopy::platform
