#include "clipboard/PasteService.h"
#include "clipboard/ClipboardWriter.h"
#include <QTimer>

namespace hopy {

namespace {
constexpr int kClipboardSettleMs = 20;  // let clipboard ownership take effect
constexpr int kPollMs = 15;             // how often to re-check the target has focus
constexpr int kMaxWaitMs = 500;         // give up polling and paste anyway after this
}

PasteService::PasteService(QObject* parent) : QObject(parent) {}

void PasteService::captureBeforeShow() {
    // Never target our own window: if hopy is already foreground (e.g. the hotkey
    // is pressed again while it is up), keep the real editor we captured earlier.
    const platform::WindowHandle fg = platform::captureForegroundWindow();
    if (fg && !platform::isOwnWindow(fg)) target_ = fg;
}

void PasteService::restoreFocus() {
    platform::restoreForegroundWindow(target_);
}

void PasteService::confirm(const ClipboardRecord& rec, ConfirmMode mode, bool plainText,
                           std::function<void()> onWritten) {
    // (1) write to the system clipboard
    ClipboardWriter::write(rec, plainText);
    if (onWritten) onWritten();

    if (mode == ConfirmMode::CopyOnly) {
        emit hideWindowRequested();
        return;
    }

    // (2) yield a frame so clipboard ownership settles, then restore focus + hide.
    // Restore the target FIRST — while we are still the foreground app the
    // SetForegroundWindow call is allowed; then hide ourselves.
    QTimer::singleShot(kClipboardSettleMs, this, [this] {
        platform::restoreForegroundWindow(target_);
        emit hideWindowRequested();
        pasteWhenFocused(0);
    });
}

void PasteService::pasteWhenFocused(int waitedMs) {
    // (3) paste only once the target really holds focus — a fixed delay sometimes
    // fired before the editor was ready, dropping the paste ("回车后没有输出").
    if (platform::isForegroundWindow(target_) || waitedMs >= kMaxWaitMs) {
        platform::sendPasteShortcut(/*plainText already applied at write*/ false);
        return;
    }
    platform::restoreForegroundWindow(target_);   // nudge again if it hasn't taken
    QTimer::singleShot(kPollMs, this, [this, waitedMs] { pasteWhenFocused(waitedMs + kPollMs); });
}

} // namespace hopy
