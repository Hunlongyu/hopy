#include "platform/InputHook.h"
#include <QWidget>
#include <QKeyEvent>
#include <QCoreApplication>

#if defined(Q_OS_WIN)
#include <windows.h>

namespace hopy::platform {
namespace {

HHOOK         g_kbHook    = nullptr;
HHOOK         g_mouseHook = nullptr;
HWINEVENTHOOK g_fgHook    = nullptr;
QWidget*      g_target    = nullptr;
HWND          g_targetWnd = nullptr;
InputHook*    g_self      = nullptr;
bool          g_down[256] = {};
bool          g_prevFgOwn = false;   // was the previously-seen foreground window one of ours?

// True when h belongs to this process — our own combobox/menu popups and modal
// dialogs are separate top-level windows. A foreground switch to one of them, or
// a click landing on one, must NOT dismiss hopy.
bool isOwnWindow(HWND h) {
    if (!h) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

// Special (non-text) keys hopy navigates with. Returns 0 for ordinary character
// keys, which are resolved to their unicode text instead.
int vkToQt(DWORD vk, bool shift) {
    switch (vk) {
    case VK_UP:     return Qt::Key_Up;
    case VK_DOWN:   return Qt::Key_Down;
    case VK_LEFT:   return Qt::Key_Left;
    case VK_RIGHT:  return Qt::Key_Right;
    case VK_RETURN: return Qt::Key_Return;
    case VK_ESCAPE: return Qt::Key_Escape;
    case VK_TAB:    return shift ? Qt::Key_Backtab : Qt::Key_Tab;
    case VK_BACK:   return Qt::Key_Backspace;
    case VK_DELETE: return Qt::Key_Delete;
    case VK_HOME:   return Qt::Key_Home;
    case VK_END:    return Qt::Key_End;
    case VK_PRIOR:  return Qt::Key_PageUp;
    case VK_NEXT:   return Qt::Key_PageDown;
    default:        return 0;
    }
}

QString charForKey(DWORD vk, DWORD scan) {
    BYTE ks[256];
    if (!GetKeyboardState(ks)) return {};
    WCHAR buf[8] = {};
    const int n = ToUnicode(vk, scan, ks, buf, 8, 0);
    if (n <= 0) return {};
    return QString::fromWCharArray(buf, n);
}

LRESULT CALLBACK kbProc(int code, WPARAM wp, LPARAM lp) {
    if (code != HC_ACTION || !g_target)
        return CallNextHookEx(nullptr, code, wp, lp);

    const auto* k  = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
    const bool down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
    const bool up   = (wp == WM_KEYUP   || wp == WM_SYSKEYUP);
    if (!down && !up)
        return CallNextHookEx(nullptr, code, wp, lp);

    const DWORD vk = k->vkCode;
    const bool ctrl  = GetAsyncKeyState(VK_CONTROL) & 0x8000;
    const bool alt   = GetAsyncKeyState(VK_MENU)    & 0x8000;
    const bool win   = (GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
    const bool shift = GetAsyncKeyState(VK_SHIFT)   & 0x8000;

    // Let modifier chords (the global hotkey, system shortcuts) and lone
    // modifiers through — hopy only owns unmodified / Shift-only keys.
    switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU:    case VK_LMENU:    case VK_RMENU:
    case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
    case VK_LWIN:    case VK_RWIN:     case VK_CAPITAL:
        return CallNextHookEx(nullptr, code, wp, lp);
    default: break;
    }
    if (ctrl || alt || win)
        return CallNextHookEx(nullptr, code, wp, lp);

    int qtKey = vkToQt(vk, shift);
    QString text;
    if (qtKey == 0) {                       // ordinary character key
        text = charForKey(vk, k->scanCode);
        if (text.isEmpty() || !text[0].isPrint())
            return CallNextHookEx(nullptr, code, wp, lp);   // e.g. F-keys → pass through
        qtKey = QChar(text[0].toUpper()).unicode();
    }

    const bool repeat = down && g_down[vk & 0xFF];
    g_down[vk & 0xFF] = down;
    QCoreApplication::postEvent(
        g_target,
        new QKeyEvent(down ? QEvent::KeyPress : QEvent::KeyRelease,
                      qtKey, shift ? Qt::ShiftModifier : Qt::NoModifier, text, repeat));
    return 1;   // swallow: the focused editor must not receive it
}

LRESULT CALLBACK mouseProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_self && g_targetWnd &&
        (wp == WM_LBUTTONDOWN || wp == WM_RBUTTONDOWN || wp == WM_MBUTTONDOWN)) {
        const auto* m = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        RECT r{};
        if (GetWindowRect(g_targetWnd, &r) && !PtInRect(&r, m->pt) &&
            !isOwnWindow(WindowFromPoint(m->pt)))   // a click on our own popup must not dismiss
            emit g_self->dismissRequested();   // clicked outside hopy → hide (don't swallow)
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void CALLBACK fgProc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    // Ignore our own popups/dialogs coming to the front (combobox dropdowns etc.),
    // AND the transition back when one closes: hopy is non-activating, so when an
    // own popup closes the foreground returns to whatever app was in front before
    // us — a switch-away that isn't one. Suppress the dismiss when we're leaving an
    // own window; genuine click-aways are still caught by the mouse hook.
    const bool own = isOwnWindow(hwnd);
    const bool prevOwn = g_prevFgOwn;
    g_prevFgOwn = own;
    if (g_self && !own && !prevOwn) emit g_self->dismissRequested();   // real switch to another app
}

} // namespace

InputHook::InputHook(QObject* parent) : QObject(parent) {}
InputHook::~InputHook() { stop(); }

void InputHook::start(QWidget* keyTarget) {
    g_target    = keyTarget;
    g_targetWnd = keyTarget ? reinterpret_cast<HWND>(keyTarget->winId()) : nullptr;
    g_self      = this;
    for (bool& d : g_down) d = false;
    const HMODULE mod = GetModuleHandleW(nullptr);
    if (!g_kbHook)    g_kbHook    = SetWindowsHookExW(WH_KEYBOARD_LL, kbProc,    mod, 0);
    if (!g_mouseHook) g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL,    mouseProc, mod, 0);
    if (!g_fgHook)
        g_fgHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                   nullptr, fgProc, 0, 0, WINEVENT_OUTOFCONTEXT);
}

void InputHook::stop() {
    if (g_kbHook)    { UnhookWindowsHookEx(g_kbHook);    g_kbHook    = nullptr; }
    if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
    if (g_fgHook)    { UnhookWinEvent(g_fgHook);         g_fgHook    = nullptr; }
    g_target    = nullptr;
    g_targetWnd = nullptr;
    g_self      = nullptr;
}

} // namespace hopy::platform

#else // non-Windows: keyboard-hook driving is not implemented

namespace hopy::platform {
InputHook::InputHook(QObject* parent) : QObject(parent) {}
InputHook::~InputHook() {}
void InputHook::start(QWidget*) {}
void InputHook::stop() {}
} // namespace hopy::platform

#endif
