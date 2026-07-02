#include <QtTest>
#include "util/Hash.h"

using namespace hopy;

class TestHash : public QObject {
    Q_OBJECT
private slots:
    void fnv1aIsDeterministic() {
        QCOMPARE(Hash::fnv1a(QByteArray("hello")), Hash::fnv1a(QByteArray("hello")));
    }
    void fnv1aDiffersOnInput() {
        QVERIFY(Hash::fnv1a(QByteArray("a")) != Hash::fnv1a(QByteArray("b")));
    }
    void contentHashDependsOnType() {
        const QByteArray p("same");
        QVERIFY(Hash::contentHash(ContentType::Text, p)
                != Hash::contentHash(ContentType::RichText, p));
    }
    void contentHashKnownVector() {
        // FNV-1a empty-input basis offset
        QCOMPARE(Hash::fnv1a(QByteArray()), quint64(0xcbf29ce484222325ULL));
    }
};

QTEST_APPLESS_MAIN(TestHash)
#include "test_hash.moc"
