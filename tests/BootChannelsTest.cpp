#include <QtTest>

#include "repositories/BootChannels.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/AutoStandbyStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/BatteryStore.h"
#include "stores/BluetoothStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/DashboardStore.h"
#include "stores/EngineStore.h"
#include "stores/GpsStore.h"
#include "stores/InternetStore.h"
#include "stores/ModemStore.h"
#include "stores/MotionStore.h"
#include "stores/NavigationStore.h"
#include "stores/OtaStore.h"
#include "stores/ScooterStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"
#include "stores/UsbStore.h"
#include "stores/VehicleStore.h"

class RecordingRepository : public InMemoryMdbRepository
{
public:
    QStringList registered;
    void registerPollChannel(const QString &channel, int) override
    {
        if (!registered.contains(channel))
            registered.append(channel);
    }
};

class BootChannelsTest : public QObject
{
    Q_OBJECT

private slots:
    void staticListMatchesStoreRegistration();
    void allIsStoresPlusExtras();
};

void BootChannelsTest::staticListMatchesStoreRegistration()
{
    RecordingRepository repo;
    QObject owner;
    const QList<SyncableStore *> stores{
        new EngineStore(&repo, &owner),
        new VehicleStore(&repo, &owner),
        new BatteryStore(&repo, QStringLiteral("0"), &owner),
        new BatteryStore(&repo, QStringLiteral("1"), &owner),
        new GpsStore(&repo, &owner),
        new MotionStore(&repo, &owner),
        new BluetoothStore(&repo, &owner),
        new InternetStore(&repo, &owner),
        new ModemStore(&repo, &owner),
        new NavigationStore(&repo, &owner),
        new SettingsStore(&repo, &owner),
        new OtaStore(&repo, &owner),
        new UsbStore(&repo, &owner),
        new SpeedLimitStore(&repo, &owner),
        new AutoStandbyStore(&repo, &owner),
        new ScooterStore(&repo, &owner),
        new CbBatteryStore(&repo, &owner),
        new AuxBatteryStore(&repo, &owner),
        new DashboardStore(&repo, &owner),
    };
    for (auto *store : stores)
        store->start();

    QStringList expected = BootChannels::storeChannels();
    QStringList actual = repo.registered;
    expected.sort();
    actual.sort();
    QCOMPARE(actual, expected);
}

void BootChannelsTest::allIsStoresPlusExtras()
{
    const QStringList all = BootChannels::all();
    QCOMPARE(all.size(), BootChannels::storeChannels().size()
                             + BootChannels::extraPollChannels().size());
    for (const QString &channel : BootChannels::extraPollChannels())
        QVERIFY(all.contains(channel));
    QVERIFY(!all.contains(QString()));
}

QTEST_GUILESS_MAIN(BootChannelsTest)
#include "BootChannelsTest.moc"
