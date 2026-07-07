#pragma once
#include <QWidget>
#include "storage/Record.h"

class QLabel;
class QScrollArea;

namespace hopy {

// Frameless, non-activating popup that previews a record's full content
// (full text / large image / full file paths) beside the main window. Height is
// capped to the main window height; overflow scrolls (via wheel forwarding).
class PreviewPopup : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPopup(QWidget* parent = nullptr);
    // `anchor` is the main window's visible rect (top-aligned; size capped to
    // it). `leftSide` places the popup on the left of the window, else right.
    void showPreview(const ClipboardRecord& rec, const QRect& anchor, bool leftSide);
    void setMaskSensitive(bool on) { maskSensitive_ = on; }
    void scrollByPixels(int dy);
    void page(int dir);   // dir = -1 previous page (up), +1 next page (down)
    void setOpenKeysLabel(const QString& label);   // e.g. "O / 右键", shown in the open hint

private:
    QScrollArea* scroll_ = nullptr;
    QLabel* content_ = nullptr;
    QLabel* info_ = nullptr;
    QString openKeysLabel_;
    bool maskSensitive_ = true;
};

} // namespace hopy
