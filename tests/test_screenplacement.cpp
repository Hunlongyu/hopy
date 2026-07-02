#include <QtTest>
#include "ui/ScreenPlacement.h"
using namespace hopy;

class TestScreenPlacement : public QObject {
    Q_OBJECT
    // screen 1: (0,0 1920x1080)  screen 2: (1920,0 1920x1080)
    QList<QRect> screens_{ QRect(0,0,1920,1080), QRect(1920,0,1920,1080) };
private slots:
    void picksScreenUnderCursorAndCenters() {
        QRect r = placeWindow(QPoint(2500, 500), screens_, QSize(400, 600));
        QVERIFY(screens_[1].contains(r));                              // on screen 2
        QVERIFY(qAbs(r.center().x() - screens_[1].center().x()) <= 2); // ~centered (QRect int center)
    }
    void picksFirstScreenWhenCursorThere() {
        QRect r = placeWindow(QPoint(100, 100), screens_, QSize(400, 600));
        QVERIFY(screens_[0].contains(r));                             // on screen 1
        QVERIFY(qAbs(r.center().x() - screens_[0].center().x()) <= 2);
    }
    void fallsBackToNearestWhenCursorOutside() {
        QRect r = placeWindow(QPoint(5000, 500), screens_, QSize(400, 600));
        QVERIFY(screens_[1].contains(r)); // screen 2 is nearer
    }
    void clampsWhenWindowLargerThanScreen() {
        QList<QRect> small{ QRect(0,0,300,300) };
        QRect r = placeWindow(QPoint(10,10), small, QSize(400,600));
        QCOMPARE(r.topLeft(), QPoint(0,0));
    }
    void emptyScreensReturnsAtOrigin() {
        QRect r = placeWindow(QPoint(10,10), {}, QSize(400,600));
        QCOMPARE(r.topLeft(), QPoint(0,0));
    }
};
QTEST_APPLESS_MAIN(TestScreenPlacement)
#include "test_screenplacement.moc"
