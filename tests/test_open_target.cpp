#include <QtTest>
#include <QTemporaryFile>
#include "util/OpenTarget.h"
using namespace hopy;

class TestOpenTarget : public QObject {
    Q_OBJECT
private slots:
    void detectsHttpUrl() {
        const auto r = detectOpenTarget("https://example.com");
        QCOMPARE(r.kind, OpenKind::Url);
        QCOMPARE(r.value, QStringLiteral("https://example.com"));
    }
    void bareWwwGetsHttpsPrefix() {
        const auto r = detectOpenTarget("www.example.com");
        QCOMPARE(r.kind, OpenKind::Url);
        QCOMPARE(r.value, QStringLiteral("https://www.example.com"));
    }
    void urlWithAtIsStillUrl() {
        QCOMPARE(detectOpenTarget("https://a@b.com").kind, OpenKind::Url);
    }
    void detectsEmail() {
        const auto r = detectOpenTarget("  user@example.com  ");
        QCOMPARE(r.kind, OpenKind::Email);
        QCOMPARE(r.value, QStringLiteral("user@example.com"));
    }
    void emailNeedsADot() {
        QCOMPARE(detectOpenTarget("user@host").kind, OpenKind::None);
    }
    void plainWordIsNone() {
        QCOMPARE(detectOpenTarget("hello").kind, OpenKind::None);
    }
    void nonexistentPathIsNone() {
        QCOMPARE(detectOpenTarget("Z:/no/such/path_xyz123").kind, OpenKind::None);
    }
    void existingFileIsPath() {
        QTemporaryFile f;
        QVERIFY(f.open());
        const auto r = detectOpenTarget(f.fileName());
        QCOMPARE(r.kind, OpenKind::Path);
        QVERIFY(!r.value.isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestOpenTarget)
#include "test_open_target.moc"
