#include "hotkey/GlobalHotkey.h"
#include <QHotkey>
#include <QKeySequence>

namespace hopy {

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {}
GlobalHotkey::~GlobalHotkey() = default;

bool GlobalHotkey::setShortcut(const QString& sequence) {
    // Empty = "no global hotkey": unregister and succeed (tray still works).
    if (sequence.trimmed().isEmpty()) {
        hotkey_.reset();
        return true;
    }
    // Register the new one into a temporary first; only swap it in on success so
    // a failed registration (e.g. the combo is already taken) leaves the
    // currently-working hotkey untouched.
    auto next = std::make_unique<QHotkey>(QKeySequence(sequence), true, this);
    if (!next->isRegistered()) {
        qWarning("hopy: failed to register hotkey '%s'", qPrintable(sequence));
        return false;
    }
    hotkey_ = std::move(next);
    connect(hotkey_.get(), &QHotkey::activated, this, &GlobalHotkey::activated);
    return true;
}

} // namespace hopy
