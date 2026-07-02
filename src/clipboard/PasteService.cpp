#include "clipboard/PasteService.h"
#include "clipboard/ClipboardWriter.h"
#include <QTimer>

namespace hopy {

namespace {
constexpr int kClipboardSettleMs = 20;  // let clipboard ownership take effect
constexpr int kFocusSettleMs = 60;      // wait for target window to regain focus before pasting
}

PasteService::PasteService(QObject* parent) : QObject(parent) {}

void PasteService::captureBeforeShow() {
    target_ = platform::captureForegroundWindow();
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

    // (2) yield a frame so clipboard ownership settles, then hide + restore focus
    QTimer::singleShot(kClipboardSettleMs, this, [this] {
        emit hideWindowRequested();
        platform::restoreForegroundWindow(target_);

        // (3) only after the target window regains focus, synthesize the paste key
        QTimer::singleShot(kFocusSettleMs, this, [] {
            platform::sendPasteShortcut(/*plainText already applied at write*/ false);
        });
    });
}

} // namespace hopy
