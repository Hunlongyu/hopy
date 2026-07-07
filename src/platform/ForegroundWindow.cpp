#include "platform/ForegroundWindow.h"

#if defined(Q_OS_WIN)
#include <windows.h>
namespace hopy::platform {

WindowHandle captureForegroundWindow() {
    return reinterpret_cast<WindowHandle>(GetForegroundWindow());
}

void restoreForegroundWindow(WindowHandle h) {
    if (!h) return;
    HWND target = reinterpret_cast<HWND>(h);
    if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
    // Windows blocks SetForegroundWindow once we are no longer the foreground app.
    // Attaching our input queue to the current-foreground and target threads makes
    // the call honoured, which is what reliably brings the editor back so the
    // synthesized Ctrl+V lands in it.
    const DWORD self = GetCurrentThreadId();
    HWND fg = GetForegroundWindow();
    const DWORD fgT  = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD tgtT = GetWindowThreadProcessId(target, nullptr);
    const bool aFg  = fgT  && fgT  != self && AttachThreadInput(self, fgT,  TRUE);
    const bool aTgt = tgtT && tgtT != self && tgtT != fgT && AttachThreadInput(self, tgtT, TRUE);
    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(target);
    SetForegroundWindow(target);
    if (aTgt) AttachThreadInput(self, tgtT, FALSE);
    if (aFg)  AttachThreadInput(self, fgT,  FALSE);
}

bool isForegroundWindow(WindowHandle h) {
    return h && GetForegroundWindow() == reinterpret_cast<HWND>(h);
}

bool isOwnWindow(WindowHandle h) {
    if (!h) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(h), &pid);
    return pid == GetCurrentProcessId();
}

void setNoActivate(quintptr windowHandle) {
    HWND h = reinterpret_cast<HWND>(windowHandle);
    if (!h) return;
    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
}

void sendPasteShortcut(bool plainText) {
    INPUT in[8] = {};
    int n = 0;
    auto press = [&](WORD vk, bool up) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = vk;
        in[n].ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        ++n;
    };
    press(VK_CONTROL, false);
    if (plainText) press(VK_SHIFT, false);
    press('V', false);
    press('V', true);
    if (plainText) press(VK_SHIFT, true);
    press(VK_CONTROL, true);
    SendInput(n, in, sizeof(INPUT));
}

void suppressAltMenu() {
    // A MOD_ALT global hotkey (e.g. Alt+C) has its non-modifier key eaten by
    // RegisterHotKey, so the foreground app only sees a "lone" Alt press+release —
    // which Windows treats as the menu-access key. Chromium then focuses its toolbar
    // (the ⋮ button) and the focused web <input> loses its caret, so a later paste
    // has no target. Injecting a stray Ctrl tap while Alt is still held makes the
    // pending Alt-up no longer lone, so no menu opens.
    if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) return;   // Alt not held → nothing to defeat
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = VK_CONTROL; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

bool isForegroundFullscreen() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    if (fg == GetShellWindow() || fg == GetDesktopWindow()) return false;   // the desktop isn't "fullscreen"
    if (isOwnWindow(reinterpret_cast<WindowHandle>(fg))) return false;      // never suppress over ourselves
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 63);
    if (wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Progman") == 0) return false;   // wallpaper hosts
    // Distinguish a truly-fullscreen window from a merely *maximised* one — the
    // exact check Chromium uses (chrome/browser/fullscreen_win.cc). Geometry alone
    // can't: a maximised window's resize frame overhangs the monitor, and on a
    // taskbar-less monitor the work area IS the whole monitor, and Chromium/Electron
    // apps keep the zoomed state even in F11 fullscreen — so rect / IsZoomed /
    // WS_CAPTION all fail to separate the two. The deciding signal is the frame:
    // a maximised, resizable window keeps WS_THICKFRAME / WS_DLGFRAME (+ the
    // WS_EX_WINDOWEDGE bit); a real fullscreen window is borderless without them.
    RECT wr;
    if (!GetWindowRect(fg, &wr)) return false;
    HMONITOR mon = MonitorFromRect(&wr, MONITOR_DEFAULTTONULL);
    if (!mon) return false;                       // window sits on no monitor
    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(mon, &mi)) return false;
    // Window must cover the ENTIRE monitor: intersect with the monitor and require
    // the overlap to be the whole monitor (an overhanging maximised window still
    // passes here — the frame-style test below is what rejects it).
    RECT overlap;
    if (!IntersectRect(&overlap, &wr, &mi.rcMonitor) || !EqualRect(&overlap, &mi.rcMonitor))
        return false;
    const LONG style   = GetWindowLongW(fg, GWL_STYLE);
    const LONG exStyle = GetWindowLongW(fg, GWL_EXSTYLE);
    return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
             (exStyle & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

} // namespace hopy::platform

#elif defined(Q_OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
namespace hopy::platform {
WindowHandle captureForegroundWindow() { return 0; }
void restoreForegroundWindow(WindowHandle) {}
bool isForegroundWindow(WindowHandle) { return true; }
bool isOwnWindow(WindowHandle) { return false; }
void setNoActivate(quintptr) {}
void sendPasteShortcut(bool plainText) {
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    const CGKeyCode kV = 9;
    CGEventFlags flags = kCGEventFlagMaskCommand | (plainText ? kCGEventFlagMaskShift : 0);
    CGEventRef down = CGEventCreateKeyboardEvent(src, kV, true);
    CGEventSetFlags(down, flags);
    CGEventRef up = CGEventCreateKeyboardEvent(src, kV, false);
    CGEventSetFlags(up, flags);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    if (down) CFRelease(down);
    if (up) CFRelease(up);
    if (src) CFRelease(src);
}
void suppressAltMenu() {}
bool isForegroundFullscreen() { return false; }
} // namespace hopy::platform

#else // Linux / X11
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
namespace hopy::platform {
WindowHandle captureForegroundWindow() {
    Display* d = XOpenDisplay(nullptr);
    if (!d) return 0;
    Window focus; int revert;
    XGetInputFocus(d, &focus, &revert);
    XCloseDisplay(d);
    return static_cast<WindowHandle>(focus);
}
void restoreForegroundWindow(WindowHandle h) {
    if (!h) return;
    Display* d = XOpenDisplay(nullptr);
    if (!d) return;
    XSetInputFocus(d, static_cast<Window>(h), RevertToParent, CurrentTime);
    XFlush(d);
    XCloseDisplay(d);
}
bool isForegroundWindow(WindowHandle) { return true; }
bool isOwnWindow(WindowHandle) { return false; }
void setNoActivate(quintptr) {}
void sendPasteShortcut(bool plainText) {
    Display* d = XOpenDisplay(nullptr);
    if (!d) return;
    KeyCode ctrl  = XKeysymToKeycode(d, XK_Control_L);
    KeyCode shift = XKeysymToKeycode(d, XK_Shift_L);
    KeyCode v     = XKeysymToKeycode(d, XK_v);
    XTestFakeKeyEvent(d, ctrl, True, 0);
    if (plainText) XTestFakeKeyEvent(d, shift, True, 0);
    XTestFakeKeyEvent(d, v, True, 0);
    XTestFakeKeyEvent(d, v, False, 0);
    if (plainText) XTestFakeKeyEvent(d, shift, False, 0);
    XTestFakeKeyEvent(d, ctrl, False, 0);
    XFlush(d);
    XCloseDisplay(d);
}
void suppressAltMenu() {}
bool isForegroundFullscreen() { return false; }
} // namespace hopy::platform
#endif
