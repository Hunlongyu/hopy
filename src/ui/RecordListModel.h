#pragma once
#include <QAbstractListModel>
#include <QList>
#include "storage/Record.h"
#include "ui/TypeFilter.h"

namespace hopy {

class RecordListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        TypeRole = Qt::UserRole,
        PinnedRole,
        FavoriteRole,
        CreatedAtRole,
        ImagePathRole,
        IdRole,
        RawRole,        // full, untruncated content (e.g. file paths)
    };
    using QAbstractListModel::QAbstractListModel;

    void setRecords(const QList<ClipboardRecord>& all);
    void setFilter(ContentFilter filter, const QString& search);
    void setMaskSensitive(bool on);   // mask OS-flagged secrets in the display roles
    const ClipboardRecord* recordAt(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    void recompute();
    QList<ClipboardRecord> all_;
    QList<int> visible_;          // indices into all_
    ContentFilter filter_ = ContentFilter::All;
    QString search_;
    bool maskSensitive_ = true;
};

} // namespace hopy
