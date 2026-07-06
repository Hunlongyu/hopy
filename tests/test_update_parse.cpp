#include <QtTest>
#include "update/UpdateChecker.h"
using namespace hopy;

// NOTE: plain (escaped) string literals, never R"(...)" — moc's lexer in this Qt
// build misparses the "//" inside "https://" in a raw string (treats it as a line
// comment, loses the raw-string terminator) and emits an empty .moc, breaking the
// link. Regular literals are unaffected.

class TestUpdateParse : public QObject {
    Q_OBJECT
private slots:
    void parsesTagAndBuildsAssets() {
        ReleaseInfo info;
        QVERIFY(releaseFromRedirect(
            "https://github.com/Hunlongyu/hopy/releases/tag/v0.2.0", &info));
        QCOMPARE(info.tagName, QStringLiteral("v0.2.0"));
        QVERIFY(info.hasExe());
        QVERIFY(info.hasChecksum());
        QCOMPARE(info.exeAsset.name, QStringLiteral("hopy-v0.2.0-windows-x64.exe"));
        QCOMPARE(info.exeAsset.url, QStringLiteral(
            "https://github.com/Hunlongyu/hopy/releases/download/v0.2.0/hopy-v0.2.0-windows-x64.exe"));
        QCOMPARE(info.checksumAsset.name, QStringLiteral("checksums.txt"));
        QCOMPARE(info.checksumAsset.url, QStringLiteral(
            "https://github.com/Hunlongyu/hopy/releases/download/v0.2.0/checksums.txt"));
    }
    void rejectsLocationWithoutTag() {
        ReleaseInfo info;
        QVERIFY(!releaseFromRedirect("https://github.com/Hunlongyu/hopy/releases", &info));
        QVERIFY(!releaseFromRedirect("", &info));
    }
    void toleratesTrailingSlash() {
        ReleaseInfo info;
        QVERIFY(releaseFromRedirect(
            "https://github.com/Hunlongyu/hopy/releases/tag/v1.2.3/", &info));
        QCOMPARE(info.tagName, QStringLiteral("v1.2.3"));
    }
};
QTEST_APPLESS_MAIN(TestUpdateParse)
#include "test_update_parse.moc"
