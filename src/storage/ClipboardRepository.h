#pragma once
#include <optional>
#include <QList>
#include <QStringList>
#include "storage/Database.h"
#include "storage/Record.h"

class QSqlQuery;

namespace hopy {

class ClipboardRepository {
public:
    explicit ClipboardRepository(Database db) : db_(std::move(db)) {}

    ClipboardRecord saveText(const QString& text);
    ClipboardRecord saveRichText(const QString& plain, const QString& htmlPath, const QString& rtfPath);
    ClipboardRecord saveImage(const QString& imagePath, quint64 imageHash);
    ClipboardRecord saveFiles(const QStringList& paths);

    std::optional<ClipboardRecord> getById(qint64 id) const;
    int count() const;
    QList<ClipboardRecord> recentRecords(int limit) const;

    bool touch(qint64 id);   // bump created_at to now (used after paste-back to re-sort to top)
    bool togglePin(qint64 id);
    bool toggleFavorite(qint64 id);
    bool remove(qint64 id);
    void clear();
    int cleanup(int keepCount);

private:
    ClipboardRecord upsert(qint64 id, const QString& content, ContentType type,
                           const QString& htmlPath, const QString& rtfPath,
                           const QString& imagePath);
    static ClipboardRecord fromQuery(const QSqlQuery& q);
    Database db_;
};

} // namespace hopy
