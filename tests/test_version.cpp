#include <QtTest>
#include "util/Version.h"
using namespace hopy;

class TestVersion : public QObject {
    Q_OBJECT
private slots:
    void equalVersions() {
        QCOMPARE(compareVersions("0.1.0", "0.1.0"), 0);
        QCOMPARE(compareVersions("v0.1.0", "0.1.0"), 0);   // 'v' 前缀被容忍
    }
    void ordersByMajorMinorPatch() {
        QCOMPARE(compareVersions("0.1.0", "0.2.0"), -1);
        QCOMPARE(compareVersions("0.2.0", "0.1.9"), 1);
        QCOMPARE(compareVersions("1.0.0", "0.9.9"), 1);
        QCOMPARE(compareVersions("0.1.1", "0.1.0"), 1);
    }
    void missingSegmentsAreZero() {
        QCOMPARE(compareVersions("1", "1.0.0"), 0);
        QCOMPARE(compareVersions("1.2", "1.2.0"), 0);
    }
    void ignoresPreReleaseSuffix() {
        QCOMPARE(compareVersions("v1.2.3-beta", "1.2.3"), 0);
    }
    void currentIsNonEmpty() {
        QVERIFY(!currentVersion().isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestVersion)
#include "test_version.moc"
