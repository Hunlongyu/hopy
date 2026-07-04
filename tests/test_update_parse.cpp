#include <QtTest>
#include "update/UpdateChecker.h"
using namespace hopy;

// NOTE: built as escaped literals rather than a R"(...)" raw string. moc's
// lexer in this Qt build misparses the "//" inside "https://" when it
// appears inside a raw string literal (treats it as a line comment and
// loses the raw-string terminator), which makes AUTOMOC silently emit an
// empty .moc file ("No relevant classes found") and breaks the link step
// with unresolved metaObject/qt_metacall symbols. Escaped strings avoid
// the defect; the resulting QByteArray content is unchanged.
static QByteArray sampleJson() {
    return "{"
        "\"tag_name\": \"v0.2.0\","
        "\"html_url\": \"https://github.com/Hunlongyu/hopy/releases/tag/v0.2.0\","
        "\"assets\": ["
          "{\"name\": \"hopy.exe\", \"browser_download_url\": \"https://example/hopy.exe\", \"size\": 1234},"
          "{\"name\": \"checksums.txt\", \"browser_download_url\": \"https://example/checksums.txt\", \"size\": 80}"
        "]"
      "}";
}

class TestUpdateParse : public QObject {
    Q_OBJECT
private slots:
    void parsesTagAndAssets() {
        ReleaseInfo info;
        QVERIFY(parseLatestRelease(sampleJson(), &info));
        QCOMPARE(info.tagName, QStringLiteral("v0.2.0"));
        QVERIFY(info.hasExe());
        QVERIFY(info.hasChecksum());
        QCOMPARE(info.exeAsset.name, QStringLiteral("hopy.exe"));
        QCOMPARE(info.exeAsset.url, QStringLiteral("https://example/hopy.exe"));
        QCOMPARE(info.exeAsset.size, qint64(1234));
        QCOMPARE(info.checksumAsset.name, QStringLiteral("checksums.txt"));
    }
    void rejectsInvalidJson() {
        ReleaseInfo info;
        QVERIFY(!parseLatestRelease("not json", &info));
    }
    void missingAssetsStillParsesTag() {
        ReleaseInfo info;
        QVERIFY(parseLatestRelease(R"({"tag_name":"v0.3.0","assets":[]})", &info));
        QCOMPARE(info.tagName, QStringLiteral("v0.3.0"));
        QVERIFY(!info.hasExe());
        QVERIFY(!info.hasChecksum());
    }
};
QTEST_APPLESS_MAIN(TestUpdateParse)
#include "test_update_parse.moc"
