#include <QtTest>
#include <QSqlQuery>
#include "storage/Database.h"
#include "storage/ClipboardRepository.h"

using namespace hopy;

class TestRepository : public QObject {
    Q_OBJECT
private slots:
    void migrateCreatesSchema() {
        Database db = Database::openInMemory();
        QVERIFY(db.isOpen());
        QVERIFY(db.migrate());
        QSqlQuery q(db.connection());
        QVERIFY(q.exec("SELECT COUNT(*) FROM records"));
    }

    // ── Task 4: save / dedup / query ──
    void saveTextInsertsRecord() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        auto r = repo.saveText("hello");
        QCOMPARE(repo.count(), 1);
        QCOMPARE(r.content, QString("hello"));
        QCOMPARE(r.type, ContentType::Text);
    }
    void saveTextDedupBumpsTimestampNotCount() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        auto a = repo.saveText("dup");
        QTest::qSleep(5);
        auto b = repo.saveText("dup");
        QCOMPARE(repo.count(), 1);
        QCOMPARE(a.id, b.id);
        QVERIFY(b.createdAt >= a.createdAt);
    }
    void recentReturnsNewestFirst() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        repo.saveText("first");  QTest::qSleep(5);
        repo.saveText("second"); QTest::qSleep(5);
        repo.saveText("third");
        auto list = repo.recentRecords(10);
        QCOMPARE(list.size(), 3);
        QCOMPARE(list[0].content, QString("third"));
        QCOMPARE(list[2].content, QString("first"));
    }

    // ── Task 5: pin / favorite / cleanup ──
    void togglePinPersists() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        auto r = repo.saveText("pin me");
        QVERIFY(repo.togglePin(r.id));
        QVERIFY(repo.getById(r.id)->pinned);
        QVERIFY(repo.togglePin(r.id));
        QVERIFY(!repo.getById(r.id)->pinned);
    }
    void pinnedSortsFirst() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        repo.saveText("old"); QTest::qSleep(5);
        repo.saveText("new");
        auto old = repo.recentRecords(10).last();
        repo.togglePin(old.id);
        QCOMPARE(repo.recentRecords(10).first().id, old.id);
    }
    void cleanupPreservesPinnedAndFavorite() {
        Database db = Database::openInMemory(); db.migrate();
        ClipboardRepository repo(db);
        auto a = repo.saveText("a"); QTest::qSleep(2);
        auto b = repo.saveText("b"); QTest::qSleep(2);
        auto c = repo.saveText("c"); QTest::qSleep(2);
        repo.togglePin(a.id);          // a pinned (oldest)
        repo.toggleFavorite(b.id);     // b favorited
        int removed = repo.cleanup(0); // keep 0 ordinary → only c should be deleted
        QCOMPARE(removed, 1);
        QVERIFY(repo.getById(a.id).has_value());
        QVERIFY(repo.getById(b.id).has_value());
        QVERIFY(!repo.getById(c.id).has_value());
    }
};

QTEST_GUILESS_MAIN(TestRepository)  // QCoreApplication needed for the static SQLite driver plugin
#include "test_repository.moc"
