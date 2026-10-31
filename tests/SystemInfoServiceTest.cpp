#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "services/SystemInfoService.h"

class SystemInfoServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void showsBaudAndOtaTunnelSeparately();
    void omitsRedundantBaudCapability();
    void convertsEnergyCountersToKwh();
    void updatesEnergyCountersIndependently();
    void rejectsInvalidEnergyCounters_data();
    void rejectsInvalidEnergyCounters();
    void readsBatteryEnergyWithoutEcuFallback();
    void preservesSignedNetEnergyAndCoverage();
    void marksStaleBatteryAccountingUnavailable();
};

void SystemInfoServiceTest::showsBaudAndOtaTunnelSeparately()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("system"), QStringLiteral("nrf-fw-version"), QStringLiteral("1.2.3"), false);
    repo.set(QStringLiteral("ble"), QStringLiteral("link-baud"), QStringLiteral("1000000"), false);
    repo.set(QStringLiteral("ble"), QStringLiteral("link-caps"), QStringLiteral("3"), false);

    SystemInfoService service(&repo);
    service.loadVersions();

    const QVariantList rows = service.nrfBoardRows();
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("infoFirmware"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("infoLinkBaud"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("1 Mbit/s"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("infoLinkCaps"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("OTA tunnel"));
}

void SystemInfoServiceTest::omitsRedundantBaudCapability()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("ble"), QStringLiteral("link-baud"), QStringLiteral("1000000"), false);
    repo.set(QStringLiteral("ble"), QStringLiteral("link-caps"), QStringLiteral("1"), false);

    SystemInfoService service(&repo);
    service.loadVersions();

    const QVariantList rows = service.nrfBoardRows();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("infoLinkBaud"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("1 Mbit/s"));
}

void SystemInfoServiceTest::convertsEnergyCountersToKwh()
{
    InMemoryMdbRepository repo;
    repo.set("engine-ecu", "energy:consumed", "4294967296", false);
    repo.set("engine-ecu", "energy:recovered", "1234567", false);
    SystemInfoService service(&repo);
    service.loadVersions();

    QCOMPARE(service.energyTotals().value("consumed").toDouble(), 4294.967296);
    QCOMPARE(service.energyTotals().value("recovered").toDouble(), 1.234567);
}

void SystemInfoServiceTest::updatesEnergyCountersIndependently()
{
    InMemoryMdbRepository repo;
    SystemInfoService service(&repo);
    service.loadVersions();
    QVERIFY(service.energyTotals().isEmpty());
    QSignalSpy energyChanged(&service, &SystemInfoService::energyTotalsChanged);
    QSignalSpy versionsChanged(&service, &SystemInfoService::versionRowsChanged);

    repo.set("engine-ecu", "energy:consumed", "0");
    QCOMPARE(service.energyTotals().value("consumed").toDouble(), 0.0);
    QVERIFY(!service.energyTotals().contains("recovered"));
    QCOMPARE(energyChanged.size(), 1);
    repo.set("engine-ecu", "energy:consumed", "0");
    repo.set("engine-ecu", "speed", "0");
    QCOMPARE(energyChanged.size(), 1);
    repo.set("engine-ecu", "energy:recovered", "1000000");
    QCOMPARE(service.energyTotals().value("recovered").toDouble(), 1.0);
    QCOMPARE(energyChanged.size(), 2);
    repo.set("engine-ecu", "energy:consumed", "2345678");
    QCOMPARE(service.energyTotals().value("consumed").toDouble(), 2.345678);
    QCOMPARE(service.energyTotals().value("recovered").toDouble(), 1.0);
    QCOMPARE(energyChanged.size(), 3);
    QCOMPARE(versionsChanged.size(), 0);
}

void SystemInfoServiceTest::rejectsInvalidEnergyCounters_data()
{
    QTest::addColumn<QString>("value");
    QTest::newRow("absent") << QString();
    QTest::newRow("text") << QStringLiteral("unknown");
    QTest::newRow("negative") << QStringLiteral("-1");
    QTest::newRow("fractional") << QStringLiteral("1.5");
    QTest::newRow("nan") << QStringLiteral("NaN");
    QTest::newRow("overflow") << QStringLiteral("18446744073709551616");
}

void SystemInfoServiceTest::rejectsInvalidEnergyCounters()
{
    QFETCH(QString, value);
    InMemoryMdbRepository repo;
    repo.set("engine-ecu", "energy:consumed", "1000000", false);
    repo.set("engine-ecu", "energy:recovered", "0", false);
    SystemInfoService service(&repo);
    service.loadVersions();
    QVERIFY(service.energyTotals().contains("consumed"));
    repo.set("engine-ecu", "energy:consumed", value);
    QVERIFY(!service.energyTotals().contains("consumed"));
    QVERIFY(service.energyTotals().contains("recovered"));
    QCOMPARE(service.energyTotals().value("recovered").toDouble(), 0.0);
}

void SystemInfoServiceTest::readsBatteryEnergyWithoutEcuFallback()
{
    InMemoryMdbRepository repo;
    repo.set("engine-ecu", "energy:consumed", "1000000", false);
    SystemInfoService service(&repo);
    service.loadVersions();
    QVERIFY(service.batteryEnergy().isEmpty());
    repo.set("energy", "source", "bms-capacity-delta");
    repo.set("energy", "net:mwh", "1000000");
    QVERIFY(!service.batteryEnergy().contains("net"));
    repo.set("energy", "known", "true");
    QCOMPARE(service.batteryEnergy().value("net").toDouble(), 1.0);
    repo.set("energy", "net:mwh", "NaN");
    QVERIFY(!service.batteryEnergy().contains("net"));
    repo.set("energy", "net:mwh", "1e20");
    QVERIFY(!service.batteryEnergy().contains("net"));
    repo.set("energy", "source", "unknown");
    QVERIFY(service.batteryEnergy().isEmpty());
}

void SystemInfoServiceTest::preservesSignedNetEnergyAndCoverage()
{
    InMemoryMdbRepository repo;
    repo.set("energy", "source", "bms-capacity-delta", false);
    repo.set("energy", "known", "true", false);
    repo.set("energy", "net:mwh", "-5300", false);
    repo.set("energy", "driving:mwh", "0", false);
    repo.set("energy", "gaps", "2", false);
    repo.set("energy", "since", "2026-10-31T10:00:00Z", false);
    repo.set("energy", "status", "partial", false);
    repo.set("energy", "updated-at", QString::number(QDateTime::currentMSecsSinceEpoch()), false);
    SystemInfoService service(&repo);
    service.loadVersions();
    QCOMPARE(service.batteryEnergy().value("net").toDouble(), -0.0053);
    QCOMPARE(service.batteryEnergy().value("driving").toDouble(), 0.0);
    QCOMPARE(service.batteryEnergy().value("gaps").toULongLong(), 2ULL);
    QCOMPARE(service.batteryEnergy().value("status").toString(), QStringLiteral("partial"));
    QVERIFY(!service.batteryEnergy().contains("parked"));
    QSignalSpy changed(&service, &SystemInfoService::batteryEnergyChanged);
    repo.set("energy", "net:mwh", "5300");
    QCOMPARE(changed.size(), 1);
    QCOMPARE(service.batteryEnergy().value("net").toDouble(), 0.0053);
    repo.set("energy", "known", "false");
    QVERIFY(!service.batteryEnergy().contains("net"));
}

void SystemInfoServiceTest::marksStaleBatteryAccountingUnavailable()
{
    InMemoryMdbRepository repo;
    repo.set("energy", "source", "bms-capacity-delta", false);
    repo.set("energy", "status", "collecting", false);
    repo.set("energy", "known", "true", false);
    repo.set("energy", "net:mwh", "1000", false);
    repo.set("energy", "updated-at", QString::number(QDateTime::currentMSecsSinceEpoch() - 20000), false);
    SystemInfoService service(&repo);
    service.loadVersions();
    QCOMPARE(service.batteryEnergy().value("status").toString(), QStringLiteral("unavailable"));
    QCOMPARE(service.batteryEnergy().value("net").toDouble(), 0.001);
    repo.set("energy", "updated-at", QString::number(QDateTime::currentMSecsSinceEpoch()));
    QCOMPARE(service.batteryEnergy().value("status").toString(), QStringLiteral("collecting"));
    repo.set("energy", "updated-at", QString::number(QDateTime::currentMSecsSinceEpoch() + 60000));
    QCOMPARE(service.batteryEnergy().value("status").toString(), QStringLiteral("unavailable"));
}

QTEST_GUILESS_MAIN(SystemInfoServiceTest)
#include "SystemInfoServiceTest.moc"
