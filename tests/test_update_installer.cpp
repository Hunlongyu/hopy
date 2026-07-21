#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "platform/UpdateInstaller.h"
using namespace hopy::platform;

namespace {
void writeFile(const QString& path, const QByteArray& data) {
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(data);
}
QByteArray readFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
} // namespace

class TestUpdateInstaller : public QObject {
    Q_OBJECT
private slots:
    void oldPathInsertsSuffixKeepingExtension() {
        QCOMPARE(sideBySideOldPath("C:/apps/hopy.exe"), QStringLiteral("C:/apps/hopy_old.exe"));
        QCOMPARE(sideBySideOldPath("/opt/hopy/hopy"),   QStringLiteral("/opt/hopy/hopy_old"));
    }
    void stableInstallPathNormalisesToHopyExe() {
#if defined(Q_OS_WIN)
        // A versioned download normalises to a stable, version-less hopy.exe in place.
        QCOMPARE(stableInstallPath("C:/apps/hopy-v0.4.0-windows-x64.exe"),
                 QStringLiteral("C:/apps/hopy.exe"));
        QCOMPARE(stableInstallPath("C:/apps/hopy.exe"), QStringLiteral("C:/apps/hopy.exe"));
#else
        QVERIFY(true);   // self-update / normalisation is Windows-only for now
#endif
    }
#if defined(Q_OS_WIN)
    void applyUpdateNormalisesVersionedNameToHopyExe() {
        QTemporaryDir tmp; QVERIFY(tmp.isValid());
        const QString cur      = tmp.filePath(QStringLiteral("hopy-v0.4.0-windows-x64.exe"));
        const QString incoming = tmp.filePath(QStringLiteral("downloaded.exe"));
        writeFile(cur, "OLD");
        writeFile(incoming, "NEW");

        QString installed, err;
        const auto r = applyUpdate(incoming, cur, &installed, &err);
        QVERIFY2(r == InstallResult::Ok, qPrintable(err));
        QCOMPARE(installed, tmp.filePath(QStringLiteral("hopy.exe")));
        QCOMPARE(readFile(installed), QByteArray("NEW"));    // new binary now lives under the stable name
        QVERIFY(!QFile::exists(cur));                        // the old versioned name is gone

        const QString backup = tmp.filePath(QStringLiteral("hopy-v0.4.0-windows-x64_old.exe"));
        QCOMPARE(readFile(backup), QByteArray("OLD"));       // previous binary kept as a rollback backup

        cleanupOldBinary(installed);                         // the sweep a later launch runs
        QVERIFY(!QFile::exists(backup));                     // one-time migration leftover is removed
        QVERIFY(QFile::exists(installed));                   // hopy.exe stays
    }
    void applyUpdateInPlaceKeepsHopyExe() {
        QTemporaryDir tmp; QVERIFY(tmp.isValid());
        const QString cur      = tmp.filePath(QStringLiteral("hopy.exe"));
        const QString incoming = tmp.filePath(QStringLiteral("downloaded.exe"));
        writeFile(cur, "OLD");
        writeFile(incoming, "NEW");

        QString installed, err;
        const auto r = applyUpdate(incoming, cur, &installed, &err);
        QVERIFY2(r == InstallResult::Ok, qPrintable(err));
        QCOMPARE(installed, cur);                            // already stable — stays hopy.exe
        QCOMPARE(readFile(cur), QByteArray("NEW"));
        QCOMPARE(readFile(tmp.filePath(QStringLiteral("hopy_old.exe"))), QByteArray("OLD"));
    }
#endif
};
QTEST_APPLESS_MAIN(TestUpdateInstaller)
#include "test_update_installer.moc"
