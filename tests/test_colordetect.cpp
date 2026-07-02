#include <QtTest>
#include "util/ColorDetect.h"
using namespace hopy;

class TestColorDetect : public QObject {
    Q_OBJECT
private slots:
    void hex6() {
        QCOMPARE(detectColor("bg: #303030;"), QColor(0x30, 0x30, 0x30));
    }
    void hex3Expands() {
        QCOMPARE(detectColor("color #333"), QColor(0x33, 0x33, 0x33));
    }
    void hex8HasAlpha() {
        const QColor c = detectColor("#112233ff");
        QCOMPARE(c, QColor(0x11, 0x22, 0x33, 0xff));
    }
    void rgb() {
        QCOMPARE(detectColor("rgb(255, 85, 0)"), QColor(255, 85, 0));
    }
    void rgbaAlpha() {
        const QColor c = detectColor("rgba(0,0,0,0.5)");
        QVERIFY(c.isValid());
        QCOMPARE(c.red(), 0);
        QVERIFY(qAbs(c.alphaF() - 0.5) < 0.01);
    }
    void bareHexToken() {
        QCOMPARE(detectColor("FF5500"), QColor(0xff, 0x55, 0x00));
    }
    void ignoresLongHash() {
        // a 40-char git sha must NOT be read as a colour
        QVERIFY(!detectColor("de94f1ab973ff8ffb581d5acb4d65de94f1ab973").isValid());
    }
    void ignoresPlainText() {
        QVERIFY(!detectColor("按 git 规范提交代码").isValid());
    }
    void picksEarliest() {
        // rgb appears before the hash-like token
        QCOMPARE(detectColor("use rgb(1,2,3) not #445566"), QColor(1, 2, 3));
    }
};
QTEST_APPLESS_MAIN(TestColorDetect)
#include "test_colordetect.moc"
