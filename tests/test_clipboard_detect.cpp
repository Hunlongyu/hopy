#include <QtTest>
#include "clipboard/ClipboardMonitor.h"
using namespace hopy;

class TestClipboardDetect : public QObject {
    Q_OBJECT
private slots:
    void filesBeatsEverything() {
        QCOMPARE(preferredKind(true, true, true, true), PayloadKind::Files);
    }
    void richTextBeatsImageAndText() {
        QCOMPARE(preferredKind(false, true, true, true), PayloadKind::RichText);
    }
    void imageBeatsText() {
        QCOMPARE(preferredKind(false, false, true, true), PayloadKind::Image);
    }
    void textLast() {
        QCOMPARE(preferredKind(false, false, false, true), PayloadKind::Text);
    }
    void noneWhenEmpty() {
        QCOMPARE(preferredKind(false, false, false, false), PayloadKind::None);
    }
    void dedupSameTextBlocked() {
        QVERIFY(!shouldForwardText("x", "x"));
        QVERIFY(shouldForwardText("x", "y"));
    }
    void dedupSameHashBlocked() {
        QVERIFY(!shouldForwardHash(7, 7));
        QVERIFY(shouldForwardHash(7, 8));
    }
};
QTEST_APPLESS_MAIN(TestClipboardDetect)
#include "test_clipboard_detect.moc"
