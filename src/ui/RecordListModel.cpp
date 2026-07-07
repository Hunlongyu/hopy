#include "ui/RecordListModel.h"

namespace hopy {

void RecordListModel::setRecords(const QList<ClipboardRecord>& all) {
    beginResetModel();
    all_ = all;
    recompute();
    endResetModel();
}

void RecordListModel::setFilter(ContentFilter filter, const QString& search) {
    beginResetModel();
    filter_ = filter; search_ = search;
    recompute();
    endResetModel();
}

void RecordListModel::setMaskSensitive(bool on) {
    if (maskSensitive_ == on) return;
    maskSensitive_ = on;
    if (!visible_.isEmpty())
        emit dataChanged(index(0), index(visible_.size() - 1), {Qt::DisplayRole, RawRole});
}

void RecordListModel::recompute() {
    visible_.clear();
    for (int i = 0; i < all_.size(); ++i) {
        const ClipboardRecord& r = all_[i];
        if (!matchesFilter(filter_, r.type, r.favorite)) continue;
        if (!search_.isEmpty() && !r.content.contains(search_, Qt::CaseInsensitive)) continue;
        visible_.append(i);
    }
}

const ClipboardRecord* RecordListModel::recordAt(int row) const {
    if (row < 0 || row >= visible_.size()) return nullptr;
    return &all_[visible_[row]];
}

int RecordListModel::rowCount(const QModelIndex&) const { return visible_.size(); }

QVariant RecordListModel::data(const QModelIndex& index, int role) const {
    const ClipboardRecord* r = recordAt(index.row());
    if (!r) return {};
    switch (role) {
        case Qt::DisplayRole: {
            QString s = (maskSensitive_ && r->sensitive) ? maskSecret(r->content) : r->content;
            s.replace('\n', ' ');
            return s.left(300);
        }
        case TypeRole:      return static_cast<int>(r->type);
        case PinnedRole:    return r->pinned;
        case FavoriteRole:  return r->favorite;
        case CreatedAtRole: return r->createdAt;
        case ImagePathRole: return r->imagePath;
        case IdRole:        return r->id;
        case RawRole:       return (maskSensitive_ && r->sensitive) ? maskSecret(r->content) : r->content;
        default: return {};
    }
}

} // namespace hopy
