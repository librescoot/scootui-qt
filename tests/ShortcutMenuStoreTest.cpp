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
    Q_PROPERTY(bool hasPlan READ hasPlan NOTIFY planChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY planChanged)
    Q_PROPERTY(int stopCount READ stopCount NOTIFY planChanged)
public:
    bool hasRoute() const { return m_hasRoute; }
    bool hasPlan() const { return m_hasPlan; }
    int currentStep() const { return m_currentStep; }
    int stopCount() const { return m_stopCount; }
    Q_INVOKABLE void clearNavigation() { m_hasRoute = false; emit routeChanged(); }
    Q_INVOKABLE void skipCurrentStop() { ++skipCalls; }
    void setPlan(bool hasPlan, int step, int count)
    {
        m_hasPlan = hasPlan;
        m_currentStep = step;
        m_stopCount = count;
        emit planChanged();
    }
    int skipCalls = 0;
signals:
    void routeChanged();
    void planChanged();
    void planStateChanged();
private:
    bool m_hasRoute = true;
    bool m_hasPlan = false;
    int m_currentStep = 0;
    int m_stopCount = 0;
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
    void themeActionCyclesTheme();
    void viewActionTogglesMapAndCluster();
    void developerModeOffersDebugOverlay();
    void activeNavigationOffersOverviewAndStop();
    void planNavigationOffersSkipBetweenStops();
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

void ShortcutMenuStoreTest::themeActionCyclesTheme()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.theme"),
             QStringLiteral("auto"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    SettingsService settingsService(&repo, &settings);
    engine.start();
    vehicle.start();
    settings.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, &settings, &repo, &settingsService);

    const auto executeTheme = [&menu, &repo]() {
        menu.show();
        menu.cycle();
        menu.confirm();
        repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    };

    QCOMPARE(menu.actions().at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("view"));
    QCOMPARE(menu.actions().at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("theme"));
    executeTheme();
    QTRY_COMPARE(settings.theme(), QStringLiteral("dark"));
    executeTheme();
    QTRY_COMPARE(settings.theme(), QStringLiteral("light"));
    executeTheme();
    QTRY_COMPARE(settings.theme(), QStringLiteral("auto"));
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

void ShortcutMenuStoreTest::developerModeOffersDebugOverlay()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("scooter.developer-mode"),
             QStringLiteral("false"), false);
    repo.set(QStringLiteral("dashboard"), QStringLiteral("debug"),
             QStringLiteral("off"), false);
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
    screen.setScreen(static_cast<int>(ScootEnums::ScreenMode::MotionDebug));
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);

    repo.set(QStringLiteral("settings"), QStringLiteral("scooter.developer-mode"),
             QStringLiteral("true"));
    QTRY_VERIFY(settings.developerMode());

    ShortcutMenuStore menu(&engine, &vehicle, &screen, nullptr, nullptr, nullptr,
                           nullptr, &settings, &repo, &settingsService);

    menu.show();
    QCOMPARE(menu.actionCount(), 4);
    QCOMPARE(menu.actions().at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("debug-overlay"));
    QCOMPARE(menu.actions().at(3).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("motion-debug"));

    menu.cycle();
    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(repo.get(QStringLiteral("dashboard"), QStringLiteral("debug")),
             QStringLiteral("overlay"));

    menu.show();
    menu.cycle();
    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(repo.get(QStringLiteral("dashboard"), QStringLiteral("debug")),
             QStringLiteral("off"));

    menu.show();
    menu.cycle();
    menu.cycle();
    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::MotionDebug);
    QCOMPARE(repo.get(QStringLiteral("settings"), QStringLiteral("dashboard.mode")),
             QStringLiteral("motion-debug"));

    repo.set(QStringLiteral("settings"), QStringLiteral("scooter.developer-mode"),
             QStringLiteral("false"));
    QTRY_VERIFY(!settings.developerMode());
    QTRY_COMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);
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

void ShortcutMenuStoreTest::planNavigationOffersSkipBetweenStops()
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

    navigation.setPlan(true, 0, 2);
    menu.show();
    QCOMPARE(menu.actionCount(), 4);
    QCOMPARE(menu.actions().at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("skip-stop"));

    menu.cycle();
    menu.cycle();
    menu.confirm();
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(navigation.skipCalls, 1);

    // There is nothing to skip on the final hop, so the action disappears.
    navigation.setPlan(true, 1, 2);
    QCOMPARE(menu.actionCount(), 3);
}

QTEST_GUILESS_MAIN(ShortcutMenuStoreTest)
#include "ShortcutMenuStoreTest.moc"
