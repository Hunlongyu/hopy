#include <QtTest>
#include "config/Settings.h"
using namespace hopy;

class TestSettings : public QObject {
    Q_OBJECT
private slots:
    void roundTripJson() {
        AppSettings s; s.theme = "light"; s.maxHistory = 50; s.confirmMode = ConfirmMode::CopyOnly;
        AppSettings r = Settings::fromJson(Settings::toJson(s));
        QCOMPARE(r.theme, QString("light"));
        QCOMPARE(r.maxHistory, 50);
        QCOMPARE(r.confirmMode, ConfirmMode::CopyOnly);
    }
    void invalidThemeFallsBackToDark() {
        AppSettings r = Settings::fromJson(R"({"theme":"solarized"})");
        QCOMPARE(r.theme, QString("dark"));
    }
    void clampsHistory() {
        AppSettings r = Settings::fromJson(R"({"maxHistory": 99999})");
        QCOMPARE(r.maxHistory, 1000);
    }
    void clampsOpacity() {
        AppSettings r = Settings::fromJson(R"({"windowOpacity": 5})");
        QCOMPARE(r.windowOpacity, 60);
    }
    void emptyJsonYieldsDefaults() {
        AppSettings r = Settings::fromJson("{}");
        QCOMPARE(r.theme, QString("dark"));
        QCOMPARE(r.maxHistory, 100);
    }
};
QTEST_APPLESS_MAIN(TestSettings)
#include "test_settings.moc"
