#include <QtTest>

#include "l10n/Translations.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/ChargingSystemMonitor.h"
#include "services/ToastService.h"
#include "stores/AuxBatteryStore.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"

// Car-style charging-system warnings: main pack active and a backup charger
// reporting not-charging. AUX is level-independent; CBB reuses its SoC gate.
// Each pack announces one transient toast per off->on transition.
class ChargingSystemMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void warnsWhenAuxNotCharging();
    void staysSilentWhenAuxCharging();
    void staysSilentWithoutReportedAux();
    void staysSilentWhenMainInactive();
    void auxDebounceCancelsWhenChargingStarts();
    void auxWarnsAgainAfterConditionClears();
    void warnsWhenCbLowAndNotCharging();
    void staysSilentWhenCbCharging();
    void staysSilentWhenCbHealthy();
    void auxAndCbAnnounceIndependently();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        BatteryStore battery0{&repo, QStringLiteral("0")};
        CbBatteryStore cbBattery{&repo};
        AuxBatteryStore auxBattery{&repo};
        ToastService toast;
        Translations translations;

        void start();
    };

    static void seedMainActive(Fixture &f);
    static void seedAuxNotCharging(Fixture &f);
    static void seedCbNotCharging(Fixture &f, int charge = 20);
};

void ChargingSystemMonitorTest::Fixture::start()
{
    battery0.start();
    cbBattery.start();
    auxBattery.start();
}

void ChargingSystemMonitorTest::seedMainActive(Fixture &f)
{
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("present"), QStringLiteral("true"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("80"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("state"), QStringLiteral("active"), false);
}

void ChargingSystemMonitorTest::seedAuxNotCharging(Fixture &f)
{
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"), QStringLiteral("12500"), false);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"), false);
}

void ChargingSystemMonitorTest::seedCbNotCharging(Fixture &f, int charge)
{
    f.repo.set(QStringLiteral("cb-battery"), QStringLiteral("present"), QStringLiteral("true"), false);
    f.repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge"), QString::number(charge), false);
    f.repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"), false);
}

void ChargingSystemMonitorTest::warnsWhenAuxNotCharging()
{
    Fixture f;
    seedMainActive(f);
    seedAuxNotCharging(f);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);

    // Still true after the debounce: no repeat within the toast lifetime.
    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 1);
}

void ChargingSystemMonitorTest::staysSilentWhenAuxCharging()
{
    Fixture f;
    seedMainActive(f);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"), QStringLiteral("12500"), false);
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("float-charge"), false);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void ChargingSystemMonitorTest::staysSilentWithoutReportedAux()
{
    // Main is active but the nRF52 has never reported anything about the AUX
    // pack, so "not-charging" would be a default, not a reading.
    Fixture f;
    seedMainActive(f);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void ChargingSystemMonitorTest::staysSilentWhenMainInactive()
{
    Fixture f;
    seedAuxNotCharging(f);
    seedCbNotCharging(f);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("present"), QStringLiteral("true"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("charge"), QStringLiteral("80"), false);
    f.repo.set(QStringLiteral("battery:0"), QStringLiteral("state"), QStringLiteral("idle"), false);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void ChargingSystemMonitorTest::auxDebounceCancelsWhenChargingStarts()
{
    Fixture f;
    seedMainActive(f);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 400);

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

void ChargingSystemMonitorTest::auxWarnsAgainAfterConditionClears()
{
    Fixture f;
    seedMainActive(f);
    seedAuxNotCharging(f);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

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

void ChargingSystemMonitorTest::warnsWhenCbLowAndNotCharging()
{
    Fixture f;
    seedMainActive(f);
    seedCbNotCharging(f, 20);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);
}

void ChargingSystemMonitorTest::staysSilentWhenCbCharging()
{
    Fixture f;
    seedMainActive(f);
    seedCbNotCharging(f, 20);
    f.repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge-status"),
               QStringLiteral("charging"), false);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void ChargingSystemMonitorTest::staysSilentWhenCbHealthy()
{
    // CBB has a real fuel gauge: at 80% the charging path being idle is not
    // worth nagging about.
    Fixture f;
    seedMainActive(f);
    seedCbNotCharging(f, 80);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTest::qWait(500);
    QCOMPARE(f.toast.toasts().size(), 0);
}

void ChargingSystemMonitorTest::auxAndCbAnnounceIndependently()
{
    Fixture f;
    seedMainActive(f);
    seedAuxNotCharging(f);
    seedCbNotCharging(f, 20);
    f.start();

    ChargingSystemMonitor monitor(&f.battery0, &f.cbBattery, &f.auxBattery,
                                  &f.toast, &f.translations, nullptr, 100);

    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 2, 2000);

    // AUX recovers; CBB keeps failing. Let both transient toasts expire.
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("bulk-charge"));
    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 0, 5000);

    // AUX fails again and re-announces even though CBB is still failing (and
    // already announced) - the two latches are independent.
    f.repo.set(QStringLiteral("aux-battery"), QStringLiteral("charge-status"),
               QStringLiteral("not-charging"));
    QTRY_COMPARE_WITH_TIMEOUT(f.toast.toasts().size(), 1, 2000);
}

QTEST_GUILESS_MAIN(ChargingSystemMonitorTest)
#include "ChargingSystemMonitorTest.moc"
