#include <QtTest>

#include "l10n/Translations.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/AuxChargeMonitor.h"
#include "services/ToastService.h"
#include "stores/AuxBatteryStore.h"
#include "stores/BatteryStore.h"

// The "alternator" warning: main pack active + AUX present, but the AUX charger
// reports not-charging. Level-independent, one transient toast per transition.
class AuxChargeMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void warnsWhenMainActiveAndAuxNotCharging();
    void staysSilentWhenAuxCharging();
    void staysSilentWithoutReportedAux();
    void staysSilentWhenMainInactive();
    void debounceCancelsWhenAuxStartsCharging();
    void warnsAgainAfterConditionClears();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        BatteryStore battery0{&repo, QStringLiteral("0")};
        AuxBatteryStore auxBattery{&repo};
        ToastService toast;
        Translations translations;
    };

    void start(Fixture &f);
    void seedMainActive(Fixture &f);
    void seedAuxNotCharging(Fixture &f);
};

void AuxChargeMonitorTest::start(Fixture &f)
{
    f.battery0.start();
    f.auxBattery.start();
}

void AuxChargeMonitorTest::seedMainActive(Fixture &f)
{
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("present"), QStringLiteral("true"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("80"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("state"), QStringLiteral("active"), false);
}

void AuxChargeMonitorTest::seedAuxNotCharging(Fixture &f)
{
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"), QStringLiteral("12500"), false);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"), false);
}

void AuxChargeMonitorTest::warnsWhenMainActiveAndAuxNotCharging()
{
    Fixture f;
    seedMainActive(f);
    seedAuxNotCharging(f);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 100);

    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);

    // Still true after the debounce: no repeat within the toast lifetime.
    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 1);
}

void AuxChargeMonitorTest::staysSilentWhenAuxCharging()
{
    Fixture f;
    seedMainActive(f);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"), QStringLiteral("12500"), false);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("float-charge"), false);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void AuxChargeMonitorTest::staysSilentWithoutReportedAux()
{
    // Main is active but the nRF52 has never reported anything about the AUX
    // pack, so "not-charging" would be a default, not a reading.
    Fixture f;
    seedMainActive(f);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void AuxChargeMonitorTest::staysSilentWhenMainInactive()
{
    Fixture f;
    seedAuxNotCharging(f);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("present"), QStringLiteral("true"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("80"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("state"), QStringLiteral("idle"), false);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void AuxChargeMonitorTest::debounceCancelsWhenAuxStartsCharging()
{
    Fixture f;
    seedMainActive(f);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 400);

    // AUX starts reporting not-charging, then the charger picks up before the
    // debounce elapses - no toast.
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"), QStringLiteral("12500"));
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"));
    QTest::qWait(100);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("bulk-charge"));

    QTest::qWait(600);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void AuxChargeMonitorTest::warnsAgainAfterConditionClears()
{
    Fixture f;
    seedMainActive(f);
    seedAuxNotCharging(f);
    start(f);

    AuxChargeMonitor monitor(&f.battery0, &f.auxBattery, &f.toast, &f.translations, nullptr, 100);

    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);

    // Charger recovers: condition clears and the toast expires.
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("bulk-charge"));
    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 0, 5000);

    // Second failure announces again (one-shot per off->on transition).
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"));
    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);
}

QTEST_GUILESS_MAIN(AuxChargeMonitorTest)
#include "AuxChargeMonitorTest.moc"
