#include <QtTest>

#include "l10n/Translations.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/BackupBatteryMonitor.h"
#include "services/ToastService.h"
#include "stores/AuxBatteryStore.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/VehicleStore.h"

class BackupBatteryMonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void warningIsDismissedWhenDrivingStarts();
    void pendingWarningIsCancelledWhenDrivingStarts();
};

void BackupBatteryMonitorTest::warningIsDismissedWhenDrivingStarts()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("battery:0"), QStringLiteral("present"),
             QStringLiteral("true"), false);
    repo.set(QStringLiteral("cb-battery"), QStringLiteral("present"),
             QStringLiteral("true"), false);
    repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge"),
             QStringLiteral("20"), false);
    repo.set(QStringLiteral("aux-battery"), QStringLiteral("voltage"),
             QStringLiteral("11000"), false);

    VehicleStore vehicle(&repo);
    BatteryStore battery0(&repo, QStringLiteral("0"));
    BatteryStore battery1(&repo, QStringLiteral("1"));
    CbBatteryStore cbBattery(&repo);
    AuxBatteryStore auxBattery(&repo);
    vehicle.start();
    battery0.start();
    battery1.start();
    cbBattery.start();
    auxBattery.start();

    ToastService toast;
    Translations translations;
    BackupBatteryMonitor monitor(&battery0, &battery1, &cbBattery, &auxBattery,
                                 &vehicle, &toast, &translations);

    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("parked"));
    QTRY_COMPARE_WITH_TIMEOUT(toast.toasts().size(), 2, 2500);

    repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge"),
             QStringLiteral("19"));
    QCOMPARE(toast.toasts().size(), 2);

    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"));
    QCOMPARE(toast.toasts().size(), 0);
}

void BackupBatteryMonitorTest::pendingWarningIsCancelledWhenDrivingStarts()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("battery:0"), QStringLiteral("present"),
             QStringLiteral("true"), false);
    repo.set(QStringLiteral("cb-battery"), QStringLiteral("present"),
             QStringLiteral("true"), false);
    repo.set(QStringLiteral("cb-battery"), QStringLiteral("charge"),
             QStringLiteral("20"), false);

    VehicleStore vehicle(&repo);
    BatteryStore battery0(&repo, QStringLiteral("0"));
    BatteryStore battery1(&repo, QStringLiteral("1"));
    CbBatteryStore cbBattery(&repo);
    AuxBatteryStore auxBattery(&repo);
    vehicle.start();
    battery0.start();
    battery1.start();
    cbBattery.start();
    auxBattery.start();

    ToastService toast;
    Translations translations;
    BackupBatteryMonitor monitor(&battery0, &battery1, &cbBattery, &auxBattery,
                                 &vehicle, &toast, &translations);

    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("parked"));
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"));

    QTest::qWait(1700);
    QCOMPARE(toast.toasts().size(), 0);
}

QTEST_GUILESS_MAIN(BackupBatteryMonitorTest)
#include "BackupBatteryMonitorTest.moc"
