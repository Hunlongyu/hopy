#pragma once
#include <QtGlobal>
#include <QRect>
#include <QString>
namespace hopy::platform {
using WindowHandle = quintptr;
WindowHandle captureForegroundWindow();
void restoreForegroundWindow(WindowHandle h);
bool isForegroundWindow(WindowHandle h);   // true once h is actually the foreground window
void sendPasteShortcut(bool plainText);

// Text-caret (the blinking insertion bar) of the current foreground window, in
// NATIVE / physical screen pixels, plus the physical rect and GDI device name
// of the monitor it lives on so the Qt layer can convert to logical coords.
// Returns false when there is no usable caret (no focused text field, or the
// app draws its own caret without a Win32 one — Chromium/Electron/UWP).
// Must be called BEFORE our window steals focus, or the caret is already gone.
struct CaretInfo {
    QRect caret;      // caret rectangle, physical screen pixels
    QRect monitor;    // monitor rectangle, physical screen pixels
    QString device;   // GDI device name, e.g. "\\.\DISPLAY1"
};
bool queryCaret(CaretInfo& out);
}
