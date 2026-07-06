#pragma once
#include <QRect>

namespace hopy::platform {

// The foreground window's text caret, in NATIVE / physical screen pixels. Returned
// by queryCaret() using whichever detection method works for the focused app.
struct CaretInfo {
    QRect caret;    // caret rectangle, physical screen pixels
};

// Find the caret of the foreground window. Tries, in order: UI Automation
// (TextPattern2 / selection), the classic Win32 caret, AttachThreadInput +
// GetCaretPos, MSAA OBJID_CARET, the IME composition point, and finally the
// focused element's bounds — each result is plausibility-checked. False if none
// yields a credible caret. Windows-only; a no-op elsewhere.
bool queryCaret(CaretInfo& out);

// Register hopy as an assistive-technology client so CEF/Chromium apps build
// their accessibility tree and expose their caret. Call once at startup.
void enableUiaAccessibility();

} // namespace hopy::platform
