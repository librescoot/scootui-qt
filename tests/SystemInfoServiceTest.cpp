#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "services/SystemInfoService.h"

class SystemInfoServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void showsBaudAndOtaTunnelSeparately();
    void omitsRedundantBaudCapability();
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

QTEST_GUILESS_MAIN(SystemInfoServiceTest)
#include "SystemInfoServiceTest.moc"
