#include <QtTest>

#include "core/ShortcutMenuItems.h"
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
    Q_PROPERTY(bool hopPromptVisible READ hopPromptVisible NOTIFY hopPromptChanged)
public:
    bool hasRoute() const { return m_hasRoute; }
    bool hasPlan() const { return m_hasPlan; }
    int currentStep() const { return m_currentStep; }
    int stopCount() const { return m_stopCount; }
    bool hopPromptVisible() const { return m_hopPromptVisible; }
    Q_INVOKABLE void setHopMenuOpen(bool open) { menuOpen = open; }
    Q_INVOKABLE void keepCurrentStop() {
        ++keepCalls;
        m_hopPromptVisible = false;
        emit hopPromptChanged();
    }
    void setHopPromptVisible(bool visible) {
        m_hopPromptVisible = visible;
        emit hopPromptChanged();
    }
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
    int keepCalls = 0;
    bool menuOpen = false;
signals:
    void routeChanged();
    void planChanged();
    void planStateChanged();
    void hopPromptChanged();
private:
    bool m_hasRoute = true;
    bool m_hopPromptVisible = false;
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
    void testingAndNightlyOfferDebugActions();
    void activeNavigationOffersOverviewAndStop();
    void planNavigationOffersSkipBetweenStops();
    void pendingHopOffersKeepStopFirst();
    void configuredOrderIsPreservedAndUnavailableItemsHidden();
    void destinationTokenHiddenWithoutAvailability();
    void destinationSlotEditsFollowSwapSemantics();
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

void ShortcutMenuStoreTest::testingAndNightlyOfferDebugActions()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("scooter.developer-mode"),
             QStringLiteral("false"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.mode"),
             QStringLiteral("speedometer"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("updates.mdb.channel"),
             QStringLiteral("stable"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("updates.dbc.channel"),
             QStringLiteral("stable"), false);

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

    const auto hasAction = [&menu](const QString &kind) {
        for (const auto &action : menu.actions()) {
            if (action.toMap().value(QStringLiteral("kind")).toString() == kind)
                return true;
        }
        return false;
    };
    QVERIFY(!hasAction(QStringLiteral("debug-overlay")));
    QVERIFY(!hasAction(QStringLiteral("motion-debug")));
    screen.setScreen(static_cast<int>(ScootEnums::ScreenMode::MotionDebug));
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);

    for (const auto &channel : {QStringLiteral("testing"), QStringLiteral("nightly")}) {
        repo.set(QStringLiteral("settings"), QStringLiteral("updates.mdb.channel"), channel);
        repo.set(QStringLiteral("settings"), QStringLiteral("updates.dbc.channel"), channel);
        QTRY_VERIFY(hasAction(QStringLiteral("debug-overlay")));
        QTRY_VERIFY(hasAction(QStringLiteral("motion-debug")));
        screen.setScreen(static_cast<int>(ScootEnums::ScreenMode::MotionDebug));
        QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::MotionDebug);
        screen.setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
    }

    repo.set(QStringLiteral("settings"), QStringLiteral("updates.dbc.channel"),
             QStringLiteral("stable"));
    QTRY_VERIFY(!hasAction(QStringLiteral("motion-debug")));
    screen.setScreen(static_cast<int>(ScootEnums::ScreenMode::MotionDebug));
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);

    repo.set(QStringLiteral("settings"), QStringLiteral("updates.mdb.channel"),
             QStringLiteral("stable"));
    QTRY_VERIFY(!hasAction(QStringLiteral("debug-overlay")));

    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.mode"),
             QStringLiteral("motion-debug"));
    QCOMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::Cluster);
    repo.set(QStringLiteral("settings"), QStringLiteral("updates.mdb.channel"),
             QStringLiteral("testing"));
    repo.set(QStringLiteral("settings"), QStringLiteral("updates.dbc.channel"),
             QStringLiteral("testing"));
    QTRY_COMPARE(screen.currentScreenMode(), ScootEnums::ScreenMode::MotionDebug);
    repo.set(QStringLiteral("settings"), QStringLiteral("updates.mdb.channel"),
             QStringLiteral("stable"));
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

void ShortcutMenuStoreTest::pendingHopOffersKeepStopFirst()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.shortcut-menu.items"),
             QStringLiteral("[\"view\"]"), false);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    vehicle.start();
    settings.start();
    NavigationStub navigation;
    navigation.setPlan(true, 0, 2);
    navigation.setHopPromptVisible(true);
    ShortcutMenuStore menu(nullptr, &vehicle, nullptr, nullptr, nullptr,
                           &navigation, nullptr, &settings, &repo, nullptr);
    QCOMPARE(menu.actions().first().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("keep-stop"));
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:long-tap"));
    QVERIFY(menu.visible());
    QVERIFY(navigation.menuOpen);
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:release"));
    repo.publish(QStringLiteral("input-events"), QStringLiteral("seatbox:press"));
    QCOMPARE(navigation.keepCalls, 1);
    QVERIFY(!navigation.menuOpen);
    QVERIFY(!menu.visible());
    QCOMPARE(menu.actions().first().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("view"));
}

void ShortcutMenuStoreTest::configuredOrderIsPreservedAndUnavailableItemsHidden()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.shortcut-menu.items"),
             QStringLiteral("[\"skip-stop\",\"theme\",\"debug-overlay\",\"view\"]"), false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    SettingsService settingsService(&repo, &settings);
    engine.start();
    vehicle.start();
    settings.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, &settings, &repo, &settingsService);

    menu.show();
    // skip-stop needs an active route and debug-overlay needs developer mode;
    // the configured order holds for what remains.
    QCOMPARE(menu.actionCount(), 2);
    QCOMPARE(menu.actions().at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("theme"));
    QCOMPARE(menu.actions().at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("view"));
}

void ShortcutMenuStoreTest::destinationTokenHiddenWithoutAvailability()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.shortcut-menu.items"),
             QStringLiteral("[\"view\",\"destination:3fa85f64-5717-4562-b3fc-2c963f66afa6:home\",\"theme\"]"),
             false);

    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    SettingsService settingsService(&repo, &settings);
    engine.start();
    vehicle.start();
    settings.start();
    ShortcutMenuStore menu(&engine, &vehicle, nullptr, nullptr, nullptr, nullptr,
                           nullptr, &settings, &repo, &settingsService);

    menu.show();
    // No saved-locations store, so the destination cannot resolve.
    QCOMPARE(menu.actionCount(), 2);
    QCOMPARE(menu.actions().at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("view"));
    QCOMPARE(menu.actions().at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("theme"));
}

void ShortcutMenuStoreTest::destinationSlotEditsFollowSwapSemantics()
{
    const QString a = QStringLiteral("3fa85f64-5717-4562-b3fc-2c963f66afa6");
    const QString b = QStringLiteral("0d6c21f0-0000-4000-8000-000000000001");
    QStringList items = ShortcutMenuItems::defaultItems();

    items = ShortcutMenuItems::setDestinationSlot(items, a, 1);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), QStringList{a});
    QCOMPARE(items.last(), QStringLiteral("destination:%1:place").arg(a));

    items = ShortcutMenuItems::setDestinationSlot(items, b, 2);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), (QStringList{a, b}));

    // Assigning b to slot 1 swaps with a rather than dropping it.
    items = ShortcutMenuItems::setDestinationSlot(items, b, 1);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), (QStringList{b, a}));

    // Re-assigning a destination to its current slot keeps one token.
    items = ShortcutMenuItems::setDestinationSlot(items, b, 1);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), (QStringList{b, a}));

    items = ShortcutMenuItems::clearDestinationSlot(items, 1);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), QStringList{a});

    items = ShortcutMenuItems::setDestinationIcon(items, a, QStringLiteral("home"));
    QVERIFY(items.last().endsWith(QStringLiteral(":home")));
    items = ShortcutMenuItems::setDestinationIcon(items, a, QStringLiteral("bogus"));
    QVERIFY(items.last().endsWith(QStringLiteral(":place")));

    // A new destination taking an occupied slot replaces the holder.
    items = ShortcutMenuItems::setDestinationSlot(items, b, 1);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), QStringList{b});

    // Pruning leaves other destinations untouched.
    items = ShortcutMenuItems::setDestinationSlot(items, a, 2);
    items = ShortcutMenuItems::withoutDestination(items, b);
    QCOMPARE(ShortcutMenuItems::destinationUuids(items), QStringList{a});
}

QTEST_GUILESS_MAIN(ShortcutMenuStoreTest)
#include "ShortcutMenuStoreTest.moc"
