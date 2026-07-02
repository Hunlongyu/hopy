#pragma once
#include <QLineEdit>
#include <QKeySequence>

class QAction;

namespace hopy {

// A single-chord global-hotkey recorder (VS Code / JetBrains style):
//  * records exactly ONE combination and replaces it on each new press
//    (never accumulates a multi-chord sequence like QKeySequenceEdit does);
//  * requires at least one of Ctrl / Alt / Win — a bare key (Space, Enter,
//    a lone letter, …) is rejected so it can never hijack normal typing;
//  * Backspace / Delete clears the field, Esc cancels the edit;
//  * shows the modifiers live while they are held.
// Exposes the same keySequence()/setKeySequence()/keySequenceChanged API as
// QKeySequenceEdit so it is a drop-in replacement.
class HotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget* parent = nullptr);

    QKeySequence keySequence() const { return seq_; }
    void setKeySequence(const QKeySequence& seq);

signals:
    void keySequenceChanged(const QKeySequence& seq);

protected:
    void keyPressEvent(QKeyEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;
    void focusInEvent(QFocusEvent* ev) override;
    void focusOutEvent(QFocusEvent* ev) override;
    void changeEvent(QEvent* ev) override;   // re-tint the clear button on theme change

private:
    void commit(const QKeySequence& seq);    // store + emit if changed, refresh display
    void showModifierPreview(Qt::KeyboardModifiers mods);
    void refreshDisplay();                   // paint the committed value (or placeholder)
    void refreshClearIcon();

    QKeySequence seq_;
    QAction* clearAction_ = nullptr;
};

} // namespace hopy
