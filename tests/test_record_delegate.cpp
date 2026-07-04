#include <QtTest>
#include "ui/RecordDelegate.h"
using namespace hopy;

// The action icons (favorite/pin/delete) live in a 3-slot strip at the card's
// top-right. actionSlotAt() maps a viewport point to slot 0/1/2, or -1 when the
// point is outside the strip. Geometry mirrors RecordDelegate::paint().
// For itemRect (0,0,300,86): card=(6,3..293,82), inner=(18,13..281,72),
// actions strip = QRect(281-66, 13, 66, 20) = (215,13,66,20); slots are 22px wide.
class TestRecordDelegate : public QObject {
    Q_OBJECT
    const QRect item{0, 0, 300, 86};
private slots:
    void favoriteSlot() { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(225, 20)), 0); }
    void pinSlot()      { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(247, 20)), 1); }
    void deleteSlot()   { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(269, 20)), 2); }
    void missLeftOfStrip()  { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(100, 20)), -1); }
    void missBelowStrip()   { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(225, 60)), -1); }
    void missRightOfStrip() { QCOMPARE(RecordDelegate::actionSlotAt(item, QPoint(295, 20)), -1); }
};
QTEST_APPLESS_MAIN(TestRecordDelegate)
#include "test_record_delegate.moc"
