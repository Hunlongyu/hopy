#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <QSize>

namespace hopy {

class RecordDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;

private:
    // Thumbnail size fit into (200x100) preserving aspect ratio; cached by path.
    QSize thumbSizeFor(const QString& path) const;
    mutable QHash<QString, QSize> thumbCache_;
};

} // namespace hopy
