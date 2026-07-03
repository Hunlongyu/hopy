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
} // namespace hopy::platform
#endif
