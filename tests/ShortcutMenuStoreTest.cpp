#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "services/SettingsService.h"
#include "stores/EngineStore.h"
#include "stores/ScreenStore.h"
#include "stores/SettingsStore.h"
#include "stores/ShortcutMenuStore.h"
#include "stores/VehicleStore.h"

class NavigationStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasRoute READ hasRoute NOTIFY routeChanged)
public:
    bool hasRoute() const { return m_hasRoute; }
    Q_INVOKABLE void clearNavigation() { m_hasRoute = false; emit routeChanged(); }
signals:
    void routeChanged();
private:
    bool m_hasRoute = true;
};

class MapStub : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE bool showRouteOverview() { ++overviewCalls; return true; }
    int overviewCalls = 0;
};

class ShortcutMenuStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void raisingKickstandDismissesAndStopsCycling();
    void onlyVisibleWhileReadyToDrive();
    void releaseStartsThreeSecondConfirmation();
    void closedMenuDoubleTapTogglesHazards();
    void viewActionTogglesMapAndCluster();
    void activeNavigationOffersOverviewAndStop();
};

void ShortcutMenuStoreTest::raisingKickstandDismissesAndStopsCycling()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("kickstand"),
             QStringLiteral("down"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, &repo, nullptr);

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

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, &repo, nullptr);

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

void ShortcutMenuStoreTest::releaseStartsThreeSecondConfirmation()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("kickstand"),
             QStringLiteral("down"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, &repo, nullptr);

    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:long-tap"));
    QVERIFY(menu.visible());
    QCOMPARE(menu.actionCount(), 1);
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:release"));
    QVERIFY(menu.confirming());
    QTRY_VERIFY_WITH_TIMEOUT(!menu.visible(), menu.confirmTimeoutMs() + 1500);
    QVERIFY(!menu.confirming());
}

void ShortcutMenuStoreTest::closedMenuDoubleTapTogglesHazards()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("blinker:state"),
             QStringLiteral("off"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, &repo, nullptr);

    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:double-tap"));
    QCOMPARE(vehicle.blinkerState(), static_cast<int>(ScootEnums::BlinkerState::Both));
    QVERIFY(!menu.visible());

    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:double-tap"));
    QCOMPARE(vehicle.blinkerState(), static_cast<int>(ScootEnums::BlinkerState::Off));
    QVERIFY(!menu.visible());
}

void ShortcutMenuStoreTest::viewActionTogglesMapAndCluster()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("kickstand"),
             QStringLiteral("down"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.mode"),
             QStringLiteral("speedometer"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    ScreenStore screen(&settings, &repo);
    SettingsService settingsService(&repo, &settings);
    engine.start();
    vehicle.start();
    settings.start();
    ShortcutMenuStore menu(&engine, &vehicle, &screen, nullptr, nullptr, nullptr,
                           nullptr, &settings, &repo, &settingsService);

    const auto executeView = [&menu, &repo]() {
        menu.show();
        menu.confirm();
        repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    };

    executeView();
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Map);
    QCOMPARE(repo.get(QStringLiteral("settings"), QStringLiteral("dashboard.mode")),
             QStringLiteral("navigation"));
    QVERIFY(!menu.visible());

    QCOMPARE(vehicle.state(), static_cast<int>(ScootEnums::VehicleState::ReadyToDrive));
    menu.show();
    QVERIFY(menu.visible());
    menu.confirm();
    QVERIFY(menu.confirming());
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QVERIFY(!menu.visible());
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);
    QCOMPARE(repo.get(QStringLiteral("settings"), QStringLiteral("dashboard.mode")),
             QStringLiteral("speedometer"));
    QVERIFY(!menu.visible());
}

void ShortcutMenuStoreTest::activeNavigationOffersOverviewAndStop()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);

    VehicleStore vehicle(&repo);
    vehicle.start();
    NavigationStub navigation;
    MapStub map;
    ShortcutMenuStore menu(nullptr, &vehicle, nullptr, nullptr, nullptr,
                           &navigation, &map, nullptr, &repo, nullptr);

    menu.show();
    QCOMPARE(menu.actionCount(), 3);
    QCOMPARE(menu.actions().at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("view"));
    QCOMPARE(menu.actions().at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("route-overview"));
    QCOMPARE(menu.actions().at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("stop-navigation"));

    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(map.overviewCalls, 1);
    QVERIFY(navigation.hasRoute());

    menu.show();
    menu.cycle();
    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QVERIFY(!navigation.hasRoute());
    QCOMPARE(menu.actionCount(), 1);
}

QTEST_GUILESS_MAIN(ShortcutMenuStoreTest)
#include "ShortcutMenuStoreTest.moc"
