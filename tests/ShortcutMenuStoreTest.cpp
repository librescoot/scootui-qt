#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/ShortcutMenuStore.h"
#include "stores/VehicleStore.h"

class ShortcutMenuStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void raisingKickstandDismissesAndStopsCycling();
    void onlyVisibleWhileReadyToDrive();
};

void ShortcutMenuStoreTest::raisingKickstandDismissesAndStopsCycling()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("kickstand"),
             QStringLiteral("down"), false);

    VehicleStore vehicle(&repo);
    vehicle.start();
    ShortcutMenuStore menu(nullptr, &vehicle, nullptr, nullptr, &repo, nullptr);

    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:long-tap"));
    QVERIFY(menu.visible());

    repo.set(QStringLiteral("vehicle"), QStringLiteral("kickstand"),
             QStringLiteral("up"));
    QTRY_VERIFY(!menu.visible());
    QVERIFY(!menu.confirming());
    QCOMPARE(menu.selectedIndex(), 0);

    QTest::qWait(900);
    QCOMPARE(menu.selectedIndex(), 0);
}

void ShortcutMenuStoreTest::onlyVisibleWhileReadyToDrive()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("parked"), false);

    VehicleStore vehicle(&repo);
    vehicle.start();
    ShortcutMenuStore menu(nullptr, &vehicle, nullptr, nullptr, &repo, nullptr);

    menu.show();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:long-tap"));
    QVERIFY(!menu.visible());

    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"));
    QTRY_COMPARE(vehicle.state(), static_cast<int>(ScootEnums::VehicleState::ReadyToDrive));
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:long-tap"));
    QVERIFY(menu.visible());

    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("parked"));
    QTRY_VERIFY(!menu.visible());
    QTest::qWait(900);
    QCOMPARE(menu.selectedIndex(), 0);
}

QTEST_GUILESS_MAIN(ShortcutMenuStoreTest)
#include "ShortcutMenuStoreTest.moc"
