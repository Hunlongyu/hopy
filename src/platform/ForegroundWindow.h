#pragma once
#include <QtGlobal>
namespace hopy::platform {
using WindowHandle = quintptr;
WindowHandle captureForegroundWindow();
void restoreForegroundWindow(WindowHandle h);
void sendPasteShortcut(bool plainText);
}
