#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/BatteryStore.h"
#include "stores/CbBatteryStore.h"

// Scooters on v1.3.0 and older publish none of these fields, so every case is
// also a tolerance case: absent keys must keep defaults, not read as zero data.
class BatteryStoreFieldsTest : public QObject
{
    Q_OBJECT

private slots:
    void capacityAndLowSocParse();
    void absentFieldsKeepDefaults();
    void lowSocFalseAndZeroReadAsNotLow();
    void connectivityBatteryDistinguishesMissingAndZero();
};

void BatteryStoreFieldsTest::capacityAndLowSocParse()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("battery:0"), QStringLiteral("remaining-capacity"),
             QStringLiteral("37600"), false);
    repo.set(QStringLiteral("battery:0"), QStringLiteral("full-capacity"),
             QStringLiteral("47000"), false);
    repo.set(QStringLiteral("battery:0"), QStringLiteral("low-soc"),
             QStringLiteral("true"), false);

    BatteryStore store(&repo, QStringLiteral("0"));
    QCOMPARE(store.remainingCapacity(), 0);
    store.start();
    QCOMPARE(store.remainingCapacity(), 37600);
    QCOMPARE(store.fullCapacity(), 47000);
    QCOMPARE(store.lowSoc(), true);
}

void BatteryStoreFieldsTest::absentFieldsKeepDefaults()
{
    InMemoryMdbRepository repo;

    BatteryStore store(&repo, QStringLiteral("1"));
    store.start();
    QCOMPARE(store.remainingCapacity(), 0);
    QCOMPARE(store.fullCapacity(), 0);
    QCOMPARE(store.lowSoc(), false);
}

void BatteryStoreFieldsTest::lowSocFalseAndZeroReadAsNotLow()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("battery:0"), QStringLiteral("low-soc"),
             QStringLiteral("false"), false);

    BatteryStore store(&repo, QStringLiteral("0"));
    store.start();
    QCOMPARE(store.lowSoc(), false);

    // battery-service publishes %v of a bool; "0"/"1" are accepted defensively
    // like present/charge.
    repo.set(QStringLiteral("battery:0"), QStringLiteral("low-soc"), QStringLiteral("1"));
    QCOMPARE(store.lowSoc(), true);
    repo.set(QStringLiteral("battery:0"), QStringLiteral("low-soc"), QStringLiteral("0"));
    QCOMPARE(store.lowSoc(), false);
}

void BatteryStoreFieldsTest::connectivityBatteryDistinguishesMissingAndZero()
{
    InMemoryMdbRepository repo;
    CbBatteryStore store(&repo);
    store.start();
    QVERIFY(!store.currentValid());
    QVERIFY(!store.cellVoltageValid());
    QVERIFY(!store.temperatureValid());
    QVERIFY(!store.cycleCountValid());
    QVERIFY(!store.stateOfHealthValid());
    QVERIFY(!store.remainingCapacityValid());
    QVERIFY(!store.fullCapacityValid());
    repo.set("cb-battery", "current", "invalid");
    QVERIFY(!store.currentValid());
    for (const auto *field : {"current", "cell-voltage", "temperature", "cycle-count",
                              "state-of-health", "remaining-capacity", "full-capacity"})
        repo.set("cb-battery", field, "0");
    QVERIFY(store.currentValid());
    QVERIFY(store.cellVoltageValid());
    QVERIFY(store.temperatureValid());
    QVERIFY(store.cycleCountValid());
    QVERIFY(store.stateOfHealthValid());
    QVERIFY(store.remainingCapacityValid());
    QVERIFY(store.fullCapacityValid());
    QCOMPARE(store.current(), 0);
    QCOMPARE(store.cellVoltage(), 0);
}

QTEST_GUILESS_MAIN(BatteryStoreFieldsTest)
#include "BatteryStoreFieldsTest.moc"
