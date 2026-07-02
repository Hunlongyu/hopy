#include <QtTest>
#include "ui/TypeFilter.h"
using namespace hopy;

class TestTypeFilter : public QObject {
    Q_OBJECT
private slots:
    void nextCyclesForwardAndWraps() {
        QCOMPARE(nextFilter(ContentFilter::Text),      ContentFilter::Image);
        QCOMPARE(nextFilter(ContentFilter::Image),     ContentFilter::Files);
        QCOMPARE(nextFilter(ContentFilter::Files),     ContentFilter::Favorites);
        QCOMPARE(nextFilter(ContentFilter::Favorites), ContentFilter::All);
        QCOMPARE(nextFilter(ContentFilter::All),       ContentFilter::Text);
    }
    void prevCyclesBackwardAndWraps() {
        QCOMPARE(prevFilter(ContentFilter::Text),  ContentFilter::All);
        QCOMPARE(prevFilter(ContentFilter::Image), ContentFilter::Text);
        QCOMPARE(prevFilter(ContentFilter::Files), ContentFilter::Image);
    }
    void textMatchesTextAndRichText() {
        QVERIFY(matchesFilter(ContentFilter::Text, ContentType::Text, false));
        QVERIFY(matchesFilter(ContentFilter::Text, ContentType::RichText, false));
        QVERIFY(!matchesFilter(ContentFilter::Text, ContentType::Image, false));
    }
    void imageAndFilesMatchTheirType() {
        QVERIFY(matchesFilter(ContentFilter::Image, ContentType::Image, false));
        QVERIFY(matchesFilter(ContentFilter::Files, ContentType::Files, false));
        QVERIFY(!matchesFilter(ContentFilter::Image, ContentType::Files, false));
    }
    void favoritesMatchesAnyFavoritedType() {
        QVERIFY(matchesFilter(ContentFilter::Favorites, ContentType::Image, true));
        QVERIFY(!matchesFilter(ContentFilter::Favorites, ContentType::Image, false));
    }
    void allMatchesEverything() {
        QVERIFY(matchesFilter(ContentFilter::All, ContentType::Files, false));
        QVERIFY(matchesFilter(ContentFilter::All, ContentType::Text, false));
    }
};
QTEST_APPLESS_MAIN(TestTypeFilter)
#include "test_typefilter.moc"
