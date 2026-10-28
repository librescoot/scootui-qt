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

QTEST_GUILESS_MAIN(SystemInfoServiceTest)
#include "SystemInfoServiceTest.moc"
