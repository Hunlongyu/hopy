#include "clipboard/PasteService.h"
#include "clipboard/ClipboardWriter.h"
#include "platform/ForegroundWindow.h"
#include <QTimer>

namespace hopy {

namespace {
constexpr int kClipboardSettleMs = 20;  // let clipboard ownership take effect
}

PasteService::PasteService(QObject* parent) : QObject(parent) {}

void PasteService::confirm(const ClipboardRecord& rec, ConfirmMode mode, bool plainText,
                           std::function<void()> onWritten) {
    // Write to the system clipboard.
    ClipboardWriter::write(rec, plainText);
    if (onWritten) onWritten();

    // hopy never took focus (non-activating window), so the editor is still the
    // foreground window with a live caret — just hide and paste into it. No
    // foreground capture/restore or focus polling needed any more.
    emit hideWindowRequested();
    if (mode == ConfirmMode::CopyOnly) return;

    QTimer::singleShot(kClipboardSettleMs, this, [] {
        platform::sendPasteShortcut(/*plainText already applied at write*/ false);
    });
}

} // namespace hopy
