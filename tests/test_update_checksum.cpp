#include <QtTest>
#include "update/UpdateDownloader.h"
using namespace hopy;

class TestUpdateChecksum : public QObject {
    Q_OBJECT
private slots:
    void picksHashByFileNameFromSha256sumFormat() {
        const QByteArray text =
            "aaaa1111  other.zip\n"
            "bbbb2222  hopy.exe\n";
        QCOMPARE(parseChecksumFor(text, "hopy.exe"), QStringLiteral("bbbb2222"));
    }
    void singleTokenFileIsUsedDirectlyAndLowercased() {
        // A ".sha256" file often contains just the hash on its own.
        const QByteArray text = "ABCDEF0123456789\n";
        QCOMPARE(parseChecksumFor(text, "hopy.exe"), QStringLiteral("abcdef0123456789"));
    }
    void returnsEmptyWhenNotFound() {
        QCOMPARE(parseChecksumFor("ffff9999  somethingelse.exe\n", "hopy.exe"), QString());
    }
    void isCaseInsensitiveOnName() {
        QCOMPARE(parseChecksumFor("dead  HOPY.EXE\n", "hopy.exe"), QStringLiteral("dead"));
    }
};
QTEST_APPLESS_MAIN(TestUpdateChecksum)
#include "test_update_checksum.moc"
