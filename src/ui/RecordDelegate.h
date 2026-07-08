#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <QSize>
#include <QRect>

namespace hopy {

class RecordDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;

    // Map a viewport point to the action-icon slot it hits within an item's rect:
    // 0 = favorite, 1 = pin, 2 = delete, or -1 if the point is outside the strip.
    static int actionSlotAt(const QRect& itemRect, const QPoint& pos);

    // Set which action icon is hovered (row + slot; -1/-1 = none) so paint() can
    // highlight it. Returns true if the hovered icon changed (caller repaints).
    bool setHoveredAction(int row, int slot);

signals:
    void favoriteClicked(qint64 id);
    void pinClicked(qint64 id);
    void deleteClicked(qint64 id);

protected:
    // Turns clicks on the painted action icons into the signals above.
    bool editorEvent(QEvent* ev, QAbstractItemModel* model,
                     const QStyleOptionViewItem& opt, const QModelIndex& idx) override;

private:
    // The 3-slot action-icon strip at a card's top-right (favorite/pin/delete).
    static QRect actionsRect(const QRect& itemRect);
    // Thumbnail size fit into (200x100) preserving aspect ratio; cached by path.
    QSize thumbSizeFor(const QString& path) const;
    mutable QHash<QString, QSize> thumbCache_;
    int hoverRow_ = -1;      // row whose action icon is hovered (-1 = none)
    int hoverSlot_ = -1;     // hovered action slot 0/1/2 (-1 = none)
};

} // namespace hopy
