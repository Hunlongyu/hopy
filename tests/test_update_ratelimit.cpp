#include <QtTest>
#include "update/UpdateChecker.h"
#include "app/UpdateService.h"
using namespace hopy;

class TestUpdateRateLimit : public QObject {
    Q_OBJECT
    static constexpr qint64 kDay = 24LL * 60 * 60 * 1000;
    const qint64 now = 1'700'000'000'000LL;
private slots:
    // isRateLimited: GitHub signals the primary rate limit as HTTP 403 with
    // X-RateLimit-Remaining: 0 (429 also counts).
    void http429IsRateLimited()      { QVERIFY(UpdateChecker::isRateLimited(429, "")); }
    void http403WithZeroRemaining()  { QVERIFY(UpdateChecker::isRateLimited(403, "0")); }
    void http403WithRemainingIsNot() { QVERIFY(!UpdateChecker::isRateLimited(403, "5")); }
    void http200IsNot()              { QVERIFY(!UpdateChecker::isRateLimited(200, "0")); }
    void http404IsNot()              { QVERIFY(!UpdateChecker::isRateLimited(404, "")); }

    // dueForCheck: the startup silent check runs at most once per 24h.
    void neverCheckedIsDue()   { QVERIFY(UpdateService::dueForCheck(0, now)); }
    void recentIsNotDue()      { QVERIFY(!UpdateService::dueForCheck(now - 23 * 60 * 60 * 1000, now)); }
    void oldIsDue()            { QVERIFY(UpdateService::dueForCheck(now - 25 * 60 * 60 * 1000, now)); }
    void exactlyADayIsDue()    { QVERIFY(UpdateService::dueForCheck(now - kDay, now)); }
    void clockSkewIsDue()      { QVERIFY(UpdateService::dueForCheck(now + 60 * 60 * 1000, now)); }
};
QTEST_APPLESS_MAIN(TestUpdateRateLimit)
#include "test_update_ratelimit.moc"
