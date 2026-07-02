#include "storage/Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QAtomicInteger>
#include <QVariant>

namespace hopy {

namespace {
constexpr int kSchemaVersion = 1;
QString nextConnectionName() {
    static QAtomicInteger<int> counter{0};
    return QStringLiteral("hopy_conn_%1").arg(counter.fetchAndAddOrdered(1));
}
} // namespace

Database Database::openInMemory() {
    const QString name = nextConnectionName();
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(":memory:");
    if (!db.open()) qWarning("hopy: failed to open in-memory db: %s",
                             qPrintable(db.lastError().text()));
    return Database(name);
}

Database Database::openAt(const QString& path) {
    const QString name = nextConnectionName();
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(path);
    if (!db.open()) qWarning("hopy: failed to open db at %s: %s",
                             qPrintable(path), qPrintable(db.lastError().text()));
    return Database(name);
}

bool Database::migrate() {
    QSqlDatabase db = connection();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)");
    q.exec("SELECT value FROM meta WHERE key='schema_version'");
    int stored = -1;
    if (q.next()) stored = q.value(0).toInt();

    if (stored != kSchemaVersion) {
        q.exec("DROP TABLE IF EXISTS records");
    }

    const bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS records ("
        " id INTEGER PRIMARY KEY,"
        " content TEXT NOT NULL,"
        " type INTEGER NOT NULL,"
        " created_at INTEGER NOT NULL,"
        " pinned INTEGER NOT NULL DEFAULT 0,"
        " favorite INTEGER NOT NULL DEFAULT 0,"
        " html_path TEXT,"
        " rtf_path TEXT,"
        " image_path TEXT)");
    if (!ok) { qWarning("hopy: create records failed: %s", qPrintable(q.lastError().text())); return false; }

    q.exec("CREATE INDEX IF NOT EXISTS idx_created ON records(created_at)");

    q.prepare("INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version', ?)");
    q.addBindValue(kSchemaVersion);
    q.exec();
    return true;
}

} // namespace hopy
