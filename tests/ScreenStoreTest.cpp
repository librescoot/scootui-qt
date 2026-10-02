#include <QtTest>

#include <functional>

#include "models/Enums.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/ScreenStore.h"
#include "stores/SettingsStore.h"

class ScreenStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void closesEveryParkedOnlyScreen();
    void returnsToOpeningScreen();
    void nestedParkedScreenReturnsToMainScreen();
    void leavesRidingScreensAlone();
    void dashboardModeIsStartupAuthority();
    void staleRemoteScreenCannotOverrideMode();
};

using Opener = std::function<void(ScreenStore &)>;

static QList<QPair<ScootEnums::ScreenMode, Opener>> parkedOnlyScreens()
{
    return {
        {ScootEnums::ScreenMode::About, [](ScreenStore &s) { s.showAbout(); }},
        {ScootEnums::ScreenMode::AddressSelection, [](ScreenStore &s) { s.showAddressSelection(); }},
        {ScootEnums::ScreenMode::NavigationSetup, [](ScreenStore &s) { s.showNavigationSetup(); }},
        {ScootEnums::ScreenMode::Faults, [](ScreenStore &s) { s.showFaults(); }},
        {ScootEnums::ScreenMode::SystemInfo, [](ScreenStore &s) { s.showSystemInfo(); }},
        {ScootEnums::ScreenMode::UpdateModeInfo, [](ScreenStore &s) { s.showUpdateModeInfo(); }},
        {ScootEnums::ScreenMode::UpdateChannel, [](ScreenStore &s) { s.showUpdateChannel(); }},
        {ScootEnums::ScreenMode::HopOnInfo, [](ScreenStore &s) { s.showHopOnInfo(); }},
        {ScootEnums::ScreenMode::KeycardEnrollInfo, [](ScreenStore &s) { s.showKeycardEnrollInfo(); }},
    };
}

void ScreenStoreTest::closesEveryParkedOnlyScreen()
{
    for (const auto &entry : parkedOnlyScreens()) {
        InMemoryMdbRepository repo;
        SettingsStore settings(&repo);
        settings.start();
        ScreenStore store(&settings, &repo);

        entry.second(store);
        QCOMPARE(store.currentScreenMode(), entry.first);
        QCOMPARE(repo.get(QStringLiteral("dashboard"), QStringLiteral("menu-open")),
                 QStringLiteral("true"));

        store.closeParkedScreens();
        QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Cluster);
        QCOMPARE(repo.get(QStringLiteral("dashboard"), QStringLiteral("menu-open")),
                 QStringLiteral("false"));
    }
}

void ScreenStoreTest::nestedParkedScreenReturnsToMainScreen()
{
    // Opened from the map, so the map must be what riding lands on even though
    // the overlay on top was opened from another overlay.
    InMemoryMdbRepository repo;
    SettingsStore settings(&repo);
    settings.start();
    ScreenStore store(&settings, &repo);

    store.setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    store.showAbout();
    store.showSystemInfo();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::SystemInfo);

    store.closeParkedScreens();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);
    QCOMPARE(repo.get(QStringLiteral("dashboard"), QStringLiteral("menu-open")),
             QStringLiteral("false"));
}

void ScreenStoreTest::returnsToOpeningScreen()
{
    InMemoryMdbRepository repo;
    SettingsStore settings(&repo);
    settings.start();
    ScreenStore store(&settings, &repo);

    store.setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    store.showAddressSelection();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::AddressSelection);

    store.closeParkedScreens();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);
}

void ScreenStoreTest::dashboardModeIsStartupAuthority()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.mode"),
             QStringLiteral("navigation"), false);
    SettingsStore settings(&repo);
    settings.start();
    ScreenStore store(&settings, &repo);

    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);
}

void ScreenStoreTest::staleRemoteScreenCannotOverrideMode()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.mode"),
             QStringLiteral("navigation"), false);
    repo.set(QStringLiteral("dashboard"), QStringLiteral("remote-screen"),
             QStringLiteral("Cluster"), false);
    SettingsStore settings(&repo);
    settings.start();
    ScreenStore store(&settings, &repo);

    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);
    repo.publish(QStringLiteral("dashboard"), QStringLiteral("remote-screen"));
    QTest::qWait(20);
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);
}

void ScreenStoreTest::leavesRidingScreensAlone()
{
    InMemoryMdbRepository repo;
    SettingsStore settings(&repo);
    settings.start();
    ScreenStore store(&settings, &repo);

    store.setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    store.closeParkedScreens();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Map);

    store.setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
    store.closeParkedScreens();
    QCOMPARE(store.currentScreenMode(), ScootEnums::ScreenMode::Cluster);
}

QTEST_MAIN(ScreenStoreTest)
#include "ScreenStoreTest.moc"