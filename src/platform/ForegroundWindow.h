#pragma once
#include <QtGlobal>

namespace hopy::platform {

using WindowHandle = quintptr;

WindowHandle captureForegroundWindow();
void restoreForegroundWindow(WindowHandle h);
bool isForegroundWindow(WindowHandle h);   // true once h is actually the foreground window
bool isOwnWindow(WindowHandle h);          // true if h belongs to our own process (don't target it)
void setNoActivate(quintptr windowHandle); // force a window to never take focus (WS_EX_NOACTIVATE)
void sendPasteShortcut(bool plainText);

} // namespace hopy::platform
