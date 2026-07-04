#include <QtTest>
#include "platform/UpdateInstaller.h"
using namespace hopy::platform;

class TestUpdateInstaller : public QObject {
    Q_OBJECT
private slots:
    void oldPathInsertsSuffixKeepingExtension() {
        QCOMPARE(sideBySideOldPath("C:/apps/hopy.exe"), QStringLiteral("C:/apps/hopy_old.exe"));
        QCOMPARE(sideBySideOldPath("/opt/hopy/hopy"),   QStringLiteral("/opt/hopy/hopy_old"));
    }
};
QTEST_APPLESS_MAIN(TestUpdateInstaller)
#include "test_update_installer.moc"
