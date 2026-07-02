#include "storage/ClipboardRepository.h"
#include "util/Hash.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>

namespace hopy {

ClipboardRecord ClipboardRepository::fromQuery(const QSqlQuery& q) {
    ClipboardRecord r;
    r.id        = q.value(0).toLongLong();
    r.content   = q.value(1).toString();
    r.type      = static_cast<ContentType>(q.value(2).toInt());
    r.createdAt = q.value(3).toLongLong();
    r.pinned    = q.value(4).toInt() != 0;
    r.favorite  = q.value(5).toInt() != 0;
    r.htmlPath  = q.value(6).toString();
    r.rtfPath   = q.value(7).toString();
    r.imagePath = q.value(8).toString();
    return r;
}

ClipboardRecord ClipboardRepository::upsert(qint64 id, const QString& content, ContentType type,
                                            const QString& htmlPath, const QString& rtfPath,
                                            const QString& imagePath) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery q(db_.connection());
    q.prepare("INSERT INTO records(id, content, type, created_at, html_path, rtf_path, image_path)"
              " VALUES(?,?,?,?,?,?,?)"
              " ON CONFLICT(id) DO UPDATE SET created_at=excluded.created_at");
    q.addBindValue(id);
    q.addBindValue(content);
    q.addBindValue(static_cast<int>(type));
    q.addBindValue(now);
    q.addBindValue(htmlPath.isEmpty() ? QVariant() : htmlPath);
    q.addBindValue(rtfPath.isEmpty()  ? QVariant() : rtfPath);
    q.addBindValue(imagePath.isEmpty()? QVariant() : imagePath);
    if (!q.exec()) qWarning("hopy: upsert failed: %s", qPrintable(q.lastError().text()));
    return getById(id).value_or(ClipboardRecord{});
}

ClipboardRecord ClipboardRepository::saveText(const QString& text) {
    const auto id = static_cast<qint64>(Hash::contentHash(ContentType::Text, text.toUtf8()));
    return upsert(id, text, ContentType::Text, {}, {}, {});
}

ClipboardRecord ClipboardRepository::saveRichText(const QString& plain, const QString& htmlPath,
                                                  const QString& rtfPath) {
    const auto id = static_cast<qint64>(Hash::contentHash(ContentType::RichText, plain.toUtf8()));
    return upsert(id, plain, ContentType::RichText, htmlPath, rtfPath, {});
}

ClipboardRecord ClipboardRepository::saveImage(const QString& imagePath, quint64 imageHash) {
    const auto id = static_cast<qint64>(imageHash);
    return upsert(id, imagePath, ContentType::Image, {}, {}, imagePath);
}

ClipboardRecord ClipboardRepository::saveFiles(const QStringList& paths) {
    const QString content = paths.join('\n');
    const auto id = static_cast<qint64>(Hash::contentHash(ContentType::Files, content.toUtf8()));
    return upsert(id, content, ContentType::Files, {}, {}, {});
}

std::optional<ClipboardRecord> ClipboardRepository::getById(qint64 id) const {
    QSqlQuery q(db_.connection());
    q.prepare("SELECT id,content,type,created_at,pinned,favorite,html_path,rtf_path,image_path"
              " FROM records WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) return fromQuery(q);
    return std::nullopt;
}

int ClipboardRepository::count() const {
    QSqlQuery q(db_.connection());
    if (q.exec("SELECT COUNT(*) FROM records") && q.next()) return q.value(0).toInt();
    return 0;
}

QList<ClipboardRecord> ClipboardRepository::recentRecords(int limit) const {
    QList<ClipboardRecord> out;
    QSqlQuery q(db_.connection());
    q.prepare("SELECT id,content,type,created_at,pinned,favorite,html_path,rtf_path,image_path"
              " FROM records ORDER BY pinned DESC, created_at DESC LIMIT ?");
    q.addBindValue(limit);
    if (q.exec()) while (q.next()) out.append(fromQuery(q));
    return out;
}

bool ClipboardRepository::touch(qint64 id) {
    QSqlQuery q(db_.connection());
    q.prepare("UPDATE records SET created_at=? WHERE id=?");
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool ClipboardRepository::togglePin(qint64 id) {
    QSqlQuery q(db_.connection());
    q.prepare("UPDATE records SET pinned = 1 - pinned WHERE id=?");
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool ClipboardRepository::toggleFavorite(qint64 id) {
    QSqlQuery q(db_.connection());
    q.prepare("UPDATE records SET favorite = 1 - favorite WHERE id=?");
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool ClipboardRepository::remove(qint64 id) {
    QSqlQuery q(db_.connection());
    q.prepare("DELETE FROM records WHERE id=?");
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

void ClipboardRepository::clear() {
    QSqlQuery q(db_.connection());
    q.exec("DELETE FROM records");
    q.exec("VACUUM");   // reclaim pages so the .sqlite file actually shrinks
}

int ClipboardRepository::cleanup(int keepCount) {
    QSqlQuery q(db_.connection());
    q.prepare(
        "DELETE FROM records WHERE pinned=0 AND favorite=0 AND id NOT IN ("
        "  SELECT id FROM records WHERE pinned=0 AND favorite=0"
        "  ORDER BY created_at DESC LIMIT ?)");
    q.addBindValue(keepCount);
    if (!q.exec()) { qWarning("hopy: cleanup failed: %s", qPrintable(q.lastError().text())); return 0; }
    return q.numRowsAffected();
}

} // namespace hopy
