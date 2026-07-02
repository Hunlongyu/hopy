#include "ui/HotkeyEdit.h"
#include "util/Icons.h"
#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>

namespace hopy {

namespace {

bool isModifierKey(int k) {
    switch (k) {
    case Qt::Key_Control: case Qt::Key_Shift:  case Qt::Key_Alt:
    case Qt::Key_Meta:    case Qt::Key_AltGr:
    case Qt::Key_Super_L: case Qt::Key_Super_R:
    case Qt::Key_Hyper_L: case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock: case Qt::Key_NumLock: case Qt::Key_ScrollLock:
        return true;
    default:
        return false;
    }
}

// Human-readable "Ctrl+Alt+…" while only modifiers are held.
QString modifierText(Qt::KeyboardModifiers mods) {
    QStringList parts;
    if (mods & Qt::ControlModifier) parts << QStringLiteral("Ctrl");
    if (mods & Qt::AltModifier)     parts << QStringLiteral("Alt");
    if (mods & Qt::MetaModifier)    parts << QStringLiteral("Win");
    if (mods & Qt::ShiftModifier)   parts << QStringLiteral("Shift");
    if (parts.isEmpty()) return {};
    return parts.join(QLatin1Char('+')) + QStringLiteral("+…");
}

} // namespace

HotkeyEdit::HotkeyEdit(QWidget* parent) : QLineEdit(parent) {
    setReadOnly(true);                       // never accept typed text
    setContextMenuPolicy(Qt::NoContextMenu); // no paste-arbitrary-text menu
    setPlaceholderText(QStringLiteral("点击后按下快捷键"));
    setToolTip(QStringLiteral("按下 Ctrl / Alt / Win 加一个键；Backspace 清空，Esc 取消"));
    setCursor(Qt::PointingHandCursor);
    // A trailing clear button that works even though the field is read-only
    // (unlike the built-in clear button, an addAction() button stays enabled).
    clearAction_ = addAction(QIcon(), QLineEdit::TrailingPosition);
    clearAction_->setToolTip(QStringLiteral("清空快捷键"));
    connect(clearAction_, &QAction::triggered, this, [this] {
        commit(QKeySequence());
        setFocus();
    });
    refreshClearIcon();
    refreshDisplay();
}

void HotkeyEdit::setKeySequence(const QKeySequence& seq) {
    seq_ = seq;
    refreshDisplay();
}

void HotkeyEdit::keyPressEvent(QKeyEvent* ev) {
    const int key = ev->key();

    if (ev->isAutoRepeat()) { ev->accept(); return; }

    // Only modifiers down so far → show them and keep waiting for the real key.
    if (isModifierKey(key)) {
        showModifierPreview(ev->modifiers());
        ev->accept();
        return;
    }

    // Esc cancels the edit, restoring the committed value.
    if (key == Qt::Key_Escape) { refreshDisplay(); clearFocus(); ev->accept(); return; }

    // Backspace / Delete clears the shortcut (they are never recordable keys).
    if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
        commit(QKeySequence());
        ev->accept();
        return;
    }

    // Enter / Tab just finish editing without recording anything.
    if (key == Qt::Key_Return || key == Qt::Key_Enter ||
        key == Qt::Key_Tab    || key == Qt::Key_Backtab) {
        refreshDisplay();
        QLineEdit::keyPressEvent(ev);   // let the focus framework move on
        return;
    }

    const Qt::KeyboardModifiers mods =
        ev->modifiers() & (Qt::ControlModifier | Qt::AltModifier |
                           Qt::MetaModifier | Qt::ShiftModifier);
    const bool hasBase = mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    // Reject bare keys and Shift-only combos — a global hotkey must carry a
    // real modifier or it would swallow ordinary keystrokes.
    if (!hasBase) {
        setText(QStringLiteral("需要 Ctrl / Alt / Win"));
        ev->accept();
        return;
    }

    commit(QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(key))));
    ev->accept();
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent* ev) {
    // If the user let go of the modifiers without pressing a real key, drop the
    // "Ctrl+…" preview and restore the committed value.
    if (!hasFocus() || (ev->modifiers() & (Qt::ControlModifier | Qt::AltModifier |
                                           Qt::MetaModifier | Qt::ShiftModifier)) == 0) {
        if (text() != seq_.toString(QKeySequence::NativeText)) refreshDisplay();
    }
    QLineEdit::keyReleaseEvent(ev);
}

void HotkeyEdit::focusInEvent(QFocusEvent* ev) {
    QLineEdit::focusInEvent(ev);
    refreshDisplay();   // ensure any stale preview is cleared
}

void HotkeyEdit::focusOutEvent(QFocusEvent* ev) {
    QLineEdit::focusOutEvent(ev);
    refreshDisplay();
}

void HotkeyEdit::changeEvent(QEvent* ev) {
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange)
        refreshClearIcon();
    QLineEdit::changeEvent(ev);
}

void HotkeyEdit::commit(const QKeySequence& seq) {
    const bool changed = (seq != seq_);
    seq_ = seq;
    refreshDisplay();
    if (changed) emit keySequenceChanged(seq_);
}

void HotkeyEdit::showModifierPreview(Qt::KeyboardModifiers mods) {
    const QString t = modifierText(mods & (Qt::ControlModifier | Qt::AltModifier |
                                           Qt::MetaModifier | Qt::ShiftModifier));
    setText(t);   // empty string falls back to the placeholder
}

void HotkeyEdit::refreshDisplay() {
    setText(seq_.isEmpty() ? QString() : seq_.toString(QKeySequence::NativeText));
    if (clearAction_) clearAction_->setVisible(!seq_.isEmpty());
}

void HotkeyEdit::refreshClearIcon() {
    if (!clearAction_) return;
    const QColor c = palette().color(QPalette::Mid);
    clearAction_->setIcon(icons::glyphIcon(icons::glyph::Delete, c, 14));
}

} // namespace hopy
