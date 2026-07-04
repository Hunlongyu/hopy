#include <QtTest>
#include <QDateTime>
#include "util/TextInfo.h"
using namespace hopy;

static qint64 ms(int y, int mo, int d, int h, int mi) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi, 0)).toMSecsSinceEpoch();
}

class TestTextInfo : public QObject {
    Q_OBJECT
    const qint64 now = ms(2026, 7, 4, 12, 0);
private slots:
    void countsCodePointsNotUtf16() {
        QCOMPARE(charCount(QStringLiteral("中文a")), 3);
        QCOMPARE(charCount(QString::fromUcs4(U"\U0001D518", 1)), 1);  // non-BMP => 1 code point
    }
    void countsLines() {
        QCOMPARE(lineCount("a\nb\nc"), 3);
        QCOMPARE(lineCount("x"), 1);
        QCOMPARE(lineCount(""), 0);
    }
    void justNowUnderAMinute() { QCOMPARE(relativeTime(now - 30'000, now), QStringLiteral("just now")); }
    void minutesAgo()          { QCOMPARE(relativeTime(now - 5*60'000, now), QStringLiteral("5 min ago")); }
    void hoursAgoSameDay()     { QCOMPARE(relativeTime(now - 3*3600'000, now), QStringLiteral("3 h ago")); }
    void yesterdayShowsClock() {
        QCOMPARE(relativeTime(ms(2026, 7, 3, 20, 0), now), QStringLiteral("Yesterday 20:00"));
    }
    void sameYearShowsMonthDay() {
        QCOMPARE(relativeTime(ms(2026, 3, 1, 9, 0), now), QStringLiteral("03-01"));
    }
    void otherYearShowsFullDate() {
        QCOMPARE(relativeTime(ms(2025, 12, 1, 9, 0), now), QStringLiteral("2025-12-01"));
    }
};
QTEST_APPLESS_MAIN(TestTextInfo)
#include "test_text_info.moc"
