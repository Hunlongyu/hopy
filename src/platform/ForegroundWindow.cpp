#include "platform/ForegroundWindow.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <uiautomation.h>
#include <oleacc.h>
#include <imm.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "imm32.lib")
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
// First bounding rectangle of a UIA text range (physical px). False if empty.
bool uiaRangeRect(IUIAutomationTextRange* r, RECT& o) {
    if (!r) return false;
    SAFEARRAY* sa = nullptr;
    bool ok = false;
    if (SUCCEEDED(r->GetBoundingRectangles(&sa)) && sa) {
        double* v = nullptr;
        long lb = 0, ub = -1;
        SafeArrayGetLBound(sa, 1, &lb);
        SafeArrayGetUBound(sa, 1, &ub);
        if (ub - lb + 1 >= 4 && SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&v)))) {
            o.left  = static_cast<LONG>(v[0]);
            o.top   = static_cast<LONG>(v[1]);
            o.right = static_cast<LONG>(v[0] + (v[2] > 0 ? v[2] : 0));
            o.bottom = static_cast<LONG>(v[1] + v[3]);
            ok = (o.bottom > o.top);
            SafeArrayUnaccessData(sa);
        }
        SafeArrayDestroy(sa);
    }
    return ok;
}

// The caret rect for a text range: the range's own rect if it has one, else
// (a collapsed caret) expand to a neighbouring character and take its edge.
bool uiaCaretFromRange(IUIAutomationTextRange* rng, RECT& out) {
    if (!rng) return false;
    RECT r{};
    if (uiaRangeRect(rng, r)) { out = r; return true; }
    IUIAutomationTextRange* c = nullptr;
    if (SUCCEEDED(rng->Clone(&c)) && c) {
        int moved = 0;
        c->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -1, &moved);
        RECT cr{};
        const bool cok = (moved != 0 && uiaRangeRect(c, cr));
        c->Release();
        if (cok) { out = RECT{ cr.right, cr.top, cr.right + 2, cr.bottom }; return true; }
    }
    if (SUCCEEDED(rng->Clone(&c)) && c) {
        int moved = 0;
        c->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, +1, &moved);
        RECT cr{};
        const bool cok = (moved != 0 && uiaRangeRect(c, cr));
        c->Release();
        if (cok) { out = RECT{ cr.left, cr.top, cr.left + 2, cr.bottom }; return true; }
    }
    return false;
}

bool caretRectViaUIA(RECT& out) {
    const HRESULT ci = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance = (ci == S_OK || ci == S_FALSE);   // don't uninit on CHANGED_MODE
    bool ok = false;
    IUIAutomation* uia = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IUIAutomation), reinterpret_cast<void**>(&uia))) && uia) {
        // Acquire a focused element that exposes TextPattern. CEF/Chromium apps
        // (DingTalk / Feishu docs) initially hand back only the outer Chromium
        // widget with no text interface until their accessibility tree is built;
        // nudge THAT window with WM_GETOBJECT and retry so the caret becomes
        // readable. Only browser widgets are worth waiting on — a plain non-text
        // focus (a button) bails immediately.
        IUIAutomationElement* el = nullptr;
        IUIAutomationTextPattern* tp = nullptr;
        for (int attempt = 0; attempt < 8; ++attempt) {
            if (el) { el->Release(); el = nullptr; }
            if (FAILED(uia->GetFocusedElement(&el)) || !el) break;
            if (SUCCEEDED(el->GetCurrentPatternAs(UIA_TextPatternId, __uuidof(IUIAutomationTextPattern),
                                                  reinterpret_cast<void**>(&tp))) && tp)
                break;                                   // got a text element
            UIA_HWND h = nullptr;
            el->get_CurrentNativeWindowHandle(&h);
            wchar_t cls[32] = {};
            if (h) GetClassNameW(reinterpret_cast<HWND>(h), cls, 32);
            if (wcsncmp(cls, L"Chrome_WidgetWin", 16) != 0) break;   // not a browser → don't wait
            DWORD_PTR r = 0;
            SendMessageTimeoutW(reinterpret_cast<HWND>(h), WM_GETOBJECT, 0, OBJID_CLIENT,
                                SMTO_ABORTIFHUNG, 120, &r);
            Sleep(35);
        }
        if (el && tp) {
            // (a) TextPattern2::GetCaretRange — the most direct, cleanest caret.
            IUIAutomationTextPattern2* tp2 = nullptr;
            if (SUCCEEDED(el->GetCurrentPatternAs(UIA_TextPattern2Id,
                    __uuidof(IUIAutomationTextPattern2), reinterpret_cast<void**>(&tp2))) && tp2) {
                BOOL activeEnd = FALSE;
                IUIAutomationTextRange* caretRange = nullptr;
                if (SUCCEEDED(tp2->GetCaretRange(&activeEnd, &caretRange)) && caretRange) {
                    if (uiaCaretFromRange(caretRange, out)) ok = true;
                    caretRange->Release();
                }
                tp2->Release();
            }
            // (b) Fallback: the selection's range (collapsed caret handled inside).
            if (!ok) {
                IUIAutomationTextRangeArray* ranges = nullptr;
                if (SUCCEEDED(tp->GetSelection(&ranges)) && ranges) {
                    int len = 0;
                    ranges->get_Length(&len);
                    IUIAutomationTextRange* rng = nullptr;
                    if (len > 0 && SUCCEEDED(ranges->GetElement(0, &rng)) && rng) {
                        if (uiaCaretFromRange(rng, out)) ok = true;
                        rng->Release();
                    }
                    ranges->Release();
                }
            }
        }
        if (tp) tp->Release();
        if (el) el->Release();
        uia->Release();
    }
    if (balance) CoUninitialize();
    return ok;
}

// Lowest-priority fallback: the focused element's top-left (the caret sits at the
// text start of an empty control). Only used when no method found a real caret.
bool caretRectViaFocusedElement(RECT& out) {
    const HRESULT ci = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance = (ci == S_OK || ci == S_FALSE);
    bool ok = false;
    IUIAutomation* uia = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IUIAutomation), reinterpret_cast<void**>(&uia))) && uia) {
        IUIAutomationElement* el = nullptr;
        if (SUCCEEDED(uia->GetFocusedElement(&el)) && el) {
            RECT er{};
            if (SUCCEEDED(el->get_CurrentBoundingRectangle(&er)) &&
                er.right > er.left && er.bottom > er.top) {
                // Only trust the element top-left when it is input-box-sized. A huge
                // element is a container / embedded Chromium widget (CEF apps like
                // DingTalk docs) whose corner is nowhere near the caret — in that
                // case return false so the caller falls back to the mouse instead.
                HWND fg = GetForegroundWindow();
                RECT wr{};
                const int elH = er.bottom - er.top;
                const int wH = (fg && GetWindowRect(fg, &wr)) ? (wr.bottom - wr.top) : 100000;
                if (elH <= 400 && elH < wH * 3 / 5) {
                    out = RECT{ er.left, er.top, er.left + 2, er.top + 24 };
                    ok = true;
                }
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
    // Reject the dummy caret many apps (Chromium, WinUI) park at the client origin
    // — must be checked BEFORE ClientToScreen, or it becomes a valid-looking corner.
    const bool bogus = (pt.x == 0 && pt.y == 0);
    if (got && !bogus) ClientToScreen(focus, &pt);
    AttachThreadInput(self, fgThread, FALSE);
    if (!got || bogus) return false;
    out = RECT{ pt.x, pt.y, pt.x + 2, pt.y + 18 };   // caret has ~0 width; nominal height
    return true;
}

// IME path: apps that support text input (incl. Chromium/CEF like DingTalk docs)
// report the caret to the IME so the candidate window draws there. Read that
// composition/candidate position — this is how Win+V positions in CEF editors.
bool caretRectViaIME(RECT& out) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    const DWORD self = GetCurrentThreadId();
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    if (!fgThread || fgThread == self) return false;
    if (!AttachThreadInput(self, fgThread, TRUE)) return false;
    HWND focus = GetFocus();
    bool ok = false;
    POINT pt{};
    DWORD compStyle = 0, candStyle = 0;
    POINT compPt{}, candPt{};
    if (focus) {
        if (HIMC himc = ImmGetContext(focus)) {
            COMPOSITIONFORM cf{};
            if (ImmGetCompositionWindow(himc, &cf)) { compStyle = cf.dwStyle; compPt = cf.ptCurrentPos; }
            CANDIDATEFORM caf{};
            if (ImmGetCandidateWindow(himc, 0, &caf)) { candStyle = caf.dwStyle; candPt = caf.ptCurrentPos; }
            // Prefer the candidate window point, else the composition point.
            if (candStyle != 0 && (candPt.x != 0 || candPt.y != 0)) { pt = candPt; ok = true; }
            else if (compStyle != 0 && (compPt.x != 0 || compPt.y != 0)) { pt = compPt; ok = true; }
            ImmReleaseContext(focus, himc);
        }
    }
    if (ok) ClientToScreen(focus, &pt);
    AttachThreadInput(self, fgThread, FALSE);
    if (!ok) return false;
    out = RECT{ pt.x, pt.y, pt.x + 2, pt.y + 18 };
    return true;
}

// MSAA caret object: the special OBJID_CARET accessible exposes the caret's
// screen rect directly — sometimes present where UIA / GetCaretPos aren't.
bool caretRectViaMSAA(RECT& out) {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    const DWORD self = GetCurrentThreadId();
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    const bool attached = (fgThread && fgThread != self && AttachThreadInput(self, fgThread, TRUE));
    HWND focus = GetFocus();
    if (!focus) focus = fg;
    bool ok = false;
    IAccessible* acc = nullptr;
    if (SUCCEEDED(AccessibleObjectFromWindow(focus, OBJID_CARET, IID_IAccessible,
                                             reinterpret_cast<void**>(&acc))) && acc) {
        VARIANT childSelf{};
        childSelf.vt = VT_I4;
        childSelf.lVal = CHILDID_SELF;
        LONG x = 0, y = 0, w = 0, h = 0;
        if (SUCCEEDED(acc->accLocation(&x, &y, &w, &h, childSelf)) && h > 0 && (x != 0 || y != 0)) {
            out = RECT{ x, y, x + (w > 0 ? w : 2), y + h };
            ok = true;
        }
        acc->Release();
    }
    if (attached) AttachThreadInput(self, fgThread, FALSE);
    return ok;
}
} // namespace

bool queryCaret(CaretInfo& out) {
    HWND fg = GetForegroundWindow();
    RECT wr{};
    if (!fg || !GetWindowRect(fg, &wr)) return false;
    // Plausibility review of a candidate caret rect. Anything that can't be a real
    // text caret is rejected so the next method is tried instead:
    //   · height must look like a text line (a few px .. ~200px), not 0 and not a
    //     whole container/screen;
    //   · not pinned to the origin (0,0) — the classic "garbage" value;
    //   · its centre must sit inside the foreground window.
    auto valid = [&](const RECT& r) {
        const int h = r.bottom - r.top;
        if (h < 4 || h > 200) return false;
        const POINT c{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        if (c.x <= 0 && c.y <= 0) return false;
        return PtInRect(&wr, c) != FALSE;
    };
    // Try each source in priority order; stop at the first that passes review.
    // Cheap classic probes come after UIA because UIA is the most universal, but
    // each returns fast when it doesn't apply.
    RECT rc{};
    if (!((caretRectViaUIA(rc)            && valid(rc)) ||   // TextPattern2 / selection (+ CEF a11y)
          (caretRectViaGuiThread(rc)      && valid(rc)) ||   // classic Win32 caret
          (caretRectViaAttachedThread(rc) && valid(rc)) ||   // attached-thread GetCaretPos
          (caretRectViaMSAA(rc)           && valid(rc)) ||   // MSAA OBJID_CARET
          (caretRectViaIME(rc)            && valid(rc)) ||   // IME composition/candidate point
          (caretRectViaFocusedElement(rc) && valid(rc))))    // rough element top-left fallback
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

namespace {
// A no-op UIA focus handler. Merely keeping it registered makes hopy look like
// an assistive-technology client, which prompts Chromium/CEF apps (DingTalk /
// Feishu docs, Electron editors) to build their accessibility tree — so their
// caret becomes queryable via UIA TextPattern, exactly how Win+V manages it.
class FocusHandler : public IUIAutomationFocusChangedEventHandler {
    LONG ref_ = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(IUIAutomationFocusChangedEventHandler)) {
            *ppv = static_cast<IUIAutomationFocusChangedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement*) override { return S_OK; }
};
IUIAutomation* g_uiaKeepAlive = nullptr;
FocusHandler*  g_focusHandler = nullptr;
} // namespace

void enableUiaAccessibility() {
    if (g_uiaKeepAlive) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IUIAutomation), reinterpret_cast<void**>(&g_uiaKeepAlive)))
        && g_uiaKeepAlive) {
        g_focusHandler = new FocusHandler();
        g_uiaKeepAlive->AddFocusChangedEventHandler(nullptr, g_focusHandler);
    }
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
void enableUiaAccessibility() {}
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
void enableUiaAccessibility() {}
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
