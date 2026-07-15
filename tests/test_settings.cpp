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
    void invalidThemeFallsBackToAuto() {
        AppSettings r = Settings::fromJson(R"({"theme":"solarized"})");
        QCOMPARE(r.theme, QString("auto"));   // default theme is "auto" (follow system)
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
        QCOMPARE(r.theme, QString("auto"));
        QCOMPARE(r.maxHistory, 100);
    }
    void openDefaultsWhenAbsent() {
        const AppSettings s = Settings::fromJson("{}");
        QCOMPARE(s.openKey, QStringLiteral("O"));
        QCOMPARE(s.openMouseButton, QStringLiteral("right"));
    }
    void openRoundTrips() {
        AppSettings s; s.openKey = "G"; s.openMouseButton = "middle";
        const AppSettings r = Settings::fromJson(Settings::toJson(s));
        QCOMPARE(r.openKey, QStringLiteral("G"));
        QCOMPARE(r.openMouseButton, QStringLiteral("middle"));
    }
    void openMouseButtonInvalidFallsBack() {
        const AppSettings s = Settings::fromJson(R"({"openMouseButton":"bogus"})");
        QCOMPARE(s.openMouseButton, QStringLiteral("right"));
    }
};
QTEST_APPLESS_MAIN(TestSettings)
#include "test_settings.moc"
