#include "platform/ForegroundWindow.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <uiautomation.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
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
namespace {
// UI Automation: the caret/selection bounding rect (physical px) of the focused
// text element. Works in Win32, WPF, WinUI/UWP and Chromium — what Win+V uses.
bool caretRectViaUIA(RECT& out) {
    const HRESULT ci = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance = (ci == S_OK || ci == S_FALSE);   // don't uninit on CHANGED_MODE
    bool ok = false;
    IUIAutomation* uia = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IUIAutomation), reinterpret_cast<void**>(&uia))) && uia) {
        IUIAutomationElement* el = nullptr;
        if (SUCCEEDED(uia->GetFocusedElement(&el)) && el) {
            IUIAutomationTextPattern* tp = nullptr;
            if (SUCCEEDED(el->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern),
                                                  reinterpret_cast<void**>(&tp))) && tp) {
                IUIAutomationTextRangeArray* ranges = nullptr;
                if (SUCCEEDED(tp->GetSelection(&ranges)) && ranges) {
                    int len = 0;
                    ranges->get_Length(&len);
                    IUIAutomationTextRange* rng = nullptr;
                    if (len > 0 && SUCCEEDED(ranges->GetElement(0, &rng)) && rng) {
                        SAFEARRAY* sa = nullptr;
                        if (SUCCEEDED(rng->GetBoundingRectangles(&sa)) && sa) {
                            double* v = nullptr;
                            long lb = 0, ub = -1;
                            SafeArrayGetLBound(sa, 1, &lb);
                            SafeArrayGetUBound(sa, 1, &ub);
                            if (ub - lb + 1 >= 4 &&
                                SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&v)))) {
                                // groups of [left, top, width, height] in physical px
                                out.left   = static_cast<LONG>(v[0]);
                                out.top    = static_cast<LONG>(v[1]);
                                out.right  = static_cast<LONG>(v[0] + (v[2] > 0 ? v[2] : 0));
                                out.bottom = static_cast<LONG>(v[1] + v[3]);
                                ok = (out.bottom > out.top);
                                SafeArrayUnaccessData(sa);
                            }
                            SafeArrayDestroy(sa);
                        }
                        rng->Release();
                    }
                    ranges->Release();
                }
                tp->Release();
            }
            el->Release();
        }
        uia->Release();
    }
    if (balance) CoUninitialize();
    return ok;
}

// Legacy fallback: the classic Win32 caret (older EDIT/RichEdit controls only).
bool caretRectViaGuiThread(RECT& out) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    GUITHREADINFO gui{};
    gui.cbSize = sizeof(gui);
    if (!GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gui) || !gui.hwndCaret)
        return false;
    RECT rc = gui.rcCaret;
    if (rc.bottom - rc.top <= 0) return false;
    POINT tl{ rc.left, rc.top }, br{ rc.right, rc.bottom };
    ClientToScreen(gui.hwndCaret, &tl);   // client → physical screen pixels
    ClientToScreen(gui.hwndCaret, &br);
    out = RECT{ tl.x, tl.y, br.x, br.y };
    return true;
}

// Last resort: attach to the foreground thread's input queue and read its caret
// directly (GetCaretPos only works for the attached thread). Catches apps whose
// caret neither UIA nor GetGUIThreadInfo reports in time.
bool caretRectViaAttachedThread(RECT& out) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    const DWORD self = GetCurrentThreadId();
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    if (!fgThread || fgThread == self) return false;
    if (!AttachThreadInput(self, fgThread, TRUE)) return false;
    POINT pt{};
    HWND focus = GetFocus();                 // the caret-owning window in that thread
    const bool got = focus && GetCaretPos(&pt);
    if (got) ClientToScreen(focus, &pt);
    AttachThreadInput(self, fgThread, FALSE);
    if (!got || (pt.x == 0 && pt.y == 0)) return false;
    out = RECT{ pt.x, pt.y, pt.x + 2, pt.y + 18 };   // caret has ~0 width; nominal height
    return true;
}
} // namespace

bool queryCaret(CaretInfo& out) {
    HWND fg = GetForegroundWindow();
    RECT wr{};
    if (!fg || !GetWindowRect(fg, &wr)) return false;
    // A genuine caret sits inside the foreground window. Validate EACH source and
    // fall through to the next, so a transitional/garbage rect from one method
    // (e.g. UIA's (0,0) right after a paste) doesn't abort the whole lookup.
    auto valid = [&](const RECT& r) {
        if (r.bottom <= r.top) return false;
        const POINT c{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        return PtInRect(&wr, c) != FALSE;
    };
    RECT rc{};
    if (!((caretRectViaUIA(rc)            && valid(rc)) ||
          (caretRectViaGuiThread(rc)      && valid(rc)) ||
          (caretRectViaAttachedThread(rc) && valid(rc))))
        return false;
    const POINT center{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
    out.caret = QRect(QPoint(rc.left, rc.top), QPoint(rc.right, rc.bottom));
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST), &mi))
        return false;
    out.monitor = QRect(QPoint(mi.rcMonitor.left, mi.rcMonitor.top),
                        QPoint(mi.rcMonitor.right, mi.rcMonitor.bottom));
    out.device = QString::fromWCharArray(mi.szDevice);
    return true;
}

void setNoActivate(quintptr windowHandle) {
    HWND h = reinterpret_cast<HWND>(windowHandle);
    if (!h) return;
    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
}

QList<MonitorInfo> physicalMonitors() {
    QList<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR h, HDC, LPRECT, LPARAM p) -> BOOL {
            auto* out = reinterpret_cast<QList<MonitorInfo>*>(p);
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(h, &mi)) {
                const RECT& r = mi.rcMonitor;
                out->append({ QRect(r.left, r.top, r.right - r.left, r.bottom - r.top),
                              QString::fromWCharArray(mi.szDevice) });
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&list));
    return list;
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
QList<MonitorInfo> physicalMonitors() { return {}; }
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
bool queryCaret(CaretInfo&) { return false; }   // caret follow: Windows-only for now
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
QList<MonitorInfo> physicalMonitors() { return {}; }
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
bool queryCaret(CaretInfo&) { return false; }   // caret follow: Windows-only for now
} // namespace hopy::platform
#endif
