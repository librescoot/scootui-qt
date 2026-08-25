#include "MenuStore.h"
#include "models/MenuNode.h"
#include "SettingsStore.h"
#include "VehicleStore.h"
#include "ThemeStore.h"
#include "TripStore.h"
#include "ScreenStore.h"
#include "SavedLocationsStore.h"
#include "RecentDestinationsStore.h"
#include "InternetStore.h"
#include "HopOnStore.h"
#include "FaultsStore.h"
#include "l10n/Translations.h"
#include "services/ToastService.h"
#include "services/SettingsService.h"
#include "services/NavigationService.h"
#include "services/NavigationAvailabilityService.h"
#include "services/MapDownloadService.h"
#include "services/UpdateChannelService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <iterator>

MenuStore::MenuStore(SettingsStore *settings, VehicleStore *vehicle,
                     ThemeStore *theme, TripStore *trip,
                     Translations *translations, SettingsService *settingsService,
                     MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_vehicle(vehicle)
    , m_theme(theme)
    , m_trip(trip)
    , m_translations(translations)
    , m_settingsService(settingsService)
    , m_repo(repo)
{
    // Rebuild menu when settings or language change
    connect(m_settings, &SettingsStore::themeChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::languageChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::blinkerStyleChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::dualBatteryChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::hornWhenSeatboxOpenChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::batteryDisplayModeChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::routePreferenceChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::avoidCobblestoneChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::valhallaUrlChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::powerDisplayModeChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmEnabledChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmHonkChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmDurationChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showGpsChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showBluetoothChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showCloudChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showInternetChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showClockChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showTemperatureChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showCbBatteryChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showAuxBatteryChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapCheckForUpdatesChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapAutoDownloadChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapViewModeChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapNorthOrientedChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::milestoneCelebrationsChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::serviceActiveChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaChannelChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaMethodChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::otaCheckIntervalChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_translations, &Translations::languageChanged, this, &MenuStore::rebuildMenuTree);

    // Close menu when vehicle starts moving
    connect(m_vehicle, &VehicleStore::stateChanged, this, [this]() {
        if (m_isOpen && !m_vehicle->isParked()) {
            close();
        }
    });

    rebuildMenuTree();

    // Reset dashboard:menu-open at startup so a prior crash can't leave it stuck at "true"
    if (m_repo) {
        m_repo->set(QStringLiteral("dashboard"),
                    QStringLiteral("menu-open"),
                    QStringLiteral("false"));
    }
}

MenuStore::~MenuStore() = default;

void MenuStore::setNavigationService(NavigationService *svc)
{
    m_navigationService = svc;
    if (m_navigationService) {
        // Both signals: nav_stop's predicate reads hasRoute(), which notifies
        // on routeChanged, and the rest of the Navigation submenu turns on
        // status. Every path that clears the route happens to change status
        // too today, so statusChanged alone would work by coincidence.
        connect(m_navigationService, &NavigationService::statusChanged,
                this, &MenuStore::rebuildMenuTree);
        connect(m_navigationService, &NavigationService::routeChanged,
                this, &MenuStore::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuStore::setSavedLocationsStore(SavedLocationsStore *store)
{
    m_savedLocations = store;
    if (m_savedLocations) {
        connect(m_savedLocations, &SavedLocationsStore::locationsChanged,
                this, &MenuStore::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuStore::setRecentDestinationsStore(RecentDestinationsStore *store)
{
    m_recentDestinations = store;
    if (m_recentDestinations) {
        connect(m_recentDestinations, &RecentDestinationsStore::destinationsChanged,
                this, &MenuStore::rebuildMenuTree);
    }
    rebuildMenuTree();
}

void MenuStore::setScreenStore(ScreenStore *store)
{
    m_screenStore = store;
}

void MenuStore::setNavigationAvailabilityService(NavigationAvailabilityService *svc)
{
    m_navAvailability = svc;
    if (m_navAvailability) {
        connect(m_navAvailability, &NavigationAvailabilityService::availabilityChanged,
                this, &MenuStore::rebuildMenuTree);
    }
}

void MenuStore::setInternetStore(InternetStore *store)
{
    m_internet = store;
}

void MenuStore::setHopOnStore(HopOnStore *store)
{
    m_hopOn = store;
    if (m_hopOn) {
        // Re-render the menu when the combo state changes (no combo <->
        // has combo flips this entry between an action and a submenu).
        connect(m_hopOn, &HopOnStore::comboChanged,
                this, &MenuStore::rebuildMenuTree);
    }
}

void MenuStore::setMapDownloadService(MapDownloadService *svc)
{
    m_mapDownload = svc;
    if (m_mapDownload) {
        connect(m_mapDownload, &MapDownloadService::updateAvailableChanged,
                this, &MenuStore::rebuildMenuTree);
        connect(m_mapDownload, &MapDownloadService::updateCheckCompleted,
                this, &MenuStore::rebuildMenuTree);
    }
}

void MenuStore::setFaultsStore(FaultsStore *store)
{
    m_faults = store;
    if (m_faults) {
        connect(m_faults, &FaultsStore::entriesChanged,
                this, &MenuStore::rebuildMenuTree);
    }
}

void MenuStore::setToastService(ToastService *svc)
{
    m_toastService = svc;
}

void MenuStore::setUpdateChannelService(UpdateChannelService *svc)
{
    m_updateChannel = svc;
}

QString MenuStore::lastMapCheckLabel() const
{
    if (!m_mapDownload)
        return {};
    return lastCheckLabel(m_mapDownload->lastUpdateCheck());
}

// Trailing "2h ago" style label for a check that last ran at iso (ISO-8601).
QString MenuStore::lastCheckLabel(const QString &iso) const
{
    if (iso.isEmpty())
        return m_translations->mapCheckNever();

    const QDateTime last = QDateTime::fromString(iso, Qt::ISODate);
    if (!last.isValid())
        return m_translations->mapCheckNever();

    // A clock that is behind the last check (boots before NTP) would otherwise
    // render as a negative age, so treat anything in the future as just now.
    const qint64 mins = last.secsTo(QDateTime::currentDateTimeUtc()) / 60;
    if (mins < 1)
        return m_translations->mapCheckJustNow();

    QString age;
    if (mins < 60)
        age = QStringLiteral("%1min").arg(mins);
    else if (mins < 60 * 24)
        age = QStringLiteral("%1h").arg(mins / 60);
    else
        age = QStringLiteral("%1d").arg(mins / (60 * 24));
    return m_translations->mapCheckAgo().arg(age);
}

void MenuStore::rebuildMenuTree()
{
    // Skip rebuilds if the menu is closed. We'll rebuild when it opens.
    if (!m_isOpen) return;

    // Skip signal-triggered rebuilds while an action is executing.
    // selectItem() will call rebuildMenuTree() once after the action completes.
    if (m_executingAction) return;

    // Store the current path to restore it if possible
    auto savedPath = m_pathStack;
    auto savedIndex = m_selectedIndex;
    auto savedIndexStack = m_indexStack;

    m_rootNode.reset(MenuNode::submenu(QStringLiteral("root"),
                                       m_translations->menuTitle(),
                                       m_translations->menuTitle()));

    auto *tr = m_translations;
    auto *svc = m_settingsService;
    auto *settings = m_settings;
    auto *repo = m_repo;

    bool isAutoTheme = settings->theme() == QLatin1String("auto");
    bool isDark = m_theme->isDark();
    QString currentLang = settings->language();

    // === Disable Service Mode (top-level, only when service mode is active) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("disable_service_mode"),
        tr->menuDisableServiceMode(), [this, repo]() {
            repo->push(QStringLiteral("settings:overlay"), QStringLiteral("clear:service"));
            close();
        }, [this]() {
            return m_settings && m_settings->serviceActive() == QLatin1String("true");
        }));

    // === Hop-on activate (top-level, only when a combo is configured) ===
    if (m_hopOn && m_hopOn->hasCombo()) {
        m_rootNode->addChild(MenuNode::action(QStringLiteral("hop_on_activate"),
            tr->menuHopOnActivateTop(), [this]() {
                close();
                m_hopOn->activate();
            }));
    }

    // === Navigation submenu (visible when display maps and routing are ready) ===
    // The header was translated while the title next to it was a literal, the
    // same mistake as the four headers below in the other direction. German
    // happens to spell this one the same way, which is why it went unnoticed.
    auto *navNode = MenuNode::submenu(QStringLiteral("navigation"),
                                       tr->menuNavigation(),
                                       tr->menuNavigationHeader(),
                                       [this]() {
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            bool routingReady = isRoutingReady();
            return (hasLocalMaps || isOnlineMap) && routingReady;
        });
    m_rootNode->addChild(navNode);

    // Enter destination code
    navNode->addChild(MenuNode::action(QStringLiteral("nav_enter_code"),
        tr->menuEnterDestinationCode(), [this]() {
            closeForScreen();
            if (m_screenStore) m_screenStore->showAddressSelection();
        }));

    // Recent destinations submenu (nested under Navigation) — last 10
    // destinations the rider has navigated to. Each gets a sub-submenu
    // with Start Navigation / Save to favorites / Delete.
    if (m_recentDestinations && m_recentDestinations->count() > 0) {
        auto *recentNode = MenuNode::submenu(QStringLiteral("recent_destinations"),
                                              tr->menuRecentDestinations());
        navNode->addChild(recentNode);

        auto dests = m_recentDestinations->destinations();
        for (const auto &destVar : dests) {
            auto dest = destVar.toMap();
            int destId = dest[QStringLiteral("id")].toInt();
            QString label = dest[QStringLiteral("label")].toString();
            if (label.isEmpty())
                label = QStringLiteral("%1, %2").arg(
                    dest[QStringLiteral("latitude")].toDouble(), 0, 'f', 5).arg(
                    dest[QStringLiteral("longitude")].toDouble(), 0, 'f', 5);

            auto *destNode = MenuNode::submenu(
                QStringLiteral("recent_dest_%1").arg(destId), label);
            recentNode->addChild(destNode);
            destNode->setPrimaryChildId(QStringLiteral("start_recent_%1").arg(destId));

            destNode->addChild(MenuNode::action(
                QStringLiteral("start_recent_%1").arg(destId),
                tr->menuStartNavigation(),
                [this, destId]() {
                    m_recentDestinations->navigateToRecent(destId);
                    close();
                }));

            destNode->addChild(MenuNode::action(
                QStringLiteral("save_recent_%1").arg(destId),
                tr->menuSaveToFavorites(),
                [this, destId]() {
                    m_recentDestinations->promoteToSaved(destId);
                }));

            destNode->addChild(MenuNode::action(
                QStringLiteral("delete_recent_%1").arg(destId),
                tr->menuDeleteLocation(),
                [this, destId]() {
                    m_recentDestinations->deleteRecent(destId);
                }));
        }
    }

    // Saved locations submenu (nested under Navigation)
    if (m_savedLocations) {
        auto *savedLocsNode = MenuNode::submenu(QStringLiteral("saved_locations"),
                                                 tr->menuSavedLocations());
        navNode->addChild(savedLocsNode);

        savedLocsNode->addChild(MenuNode::action(QStringLiteral("save_current_loc"),
            tr->menuSaveLocation(), [this]() {
                m_savedLocations->saveCurrentLocation();
                close();
            }));

        auto locs = m_savedLocations->locations();
        for (const auto &locVar : locs) {
            auto loc = locVar.toMap();
            int locId = loc[QStringLiteral("id")].toInt();
            QString label = loc[QStringLiteral("label")].toString();
            if (label.isEmpty())
                label = QStringLiteral("%1, %2").arg(
                    loc[QStringLiteral("latitude")].toDouble(), 0, 'f', 5).arg(
                    loc[QStringLiteral("longitude")].toDouble(), 0, 'f', 5);

            auto *locNode = MenuNode::submenu(
                QStringLiteral("saved_loc_%1").arg(locId), label);
            savedLocsNode->addChild(locNode);
            locNode->setPrimaryChildId(QStringLiteral("start_nav_%1").arg(locId));

            locNode->addChild(MenuNode::action(
                QStringLiteral("start_nav_%1").arg(locId),
                tr->menuStartNavigation(),
                [this, locId]() {
                    m_savedLocations->navigateToLocation(locId);
                    close();
                }));

            locNode->addChild(MenuNode::action(
                QStringLiteral("delete_loc_%1").arg(locId),
                tr->menuDeleteLocation(),
                [this, locId]() {
                    m_savedLocations->deleteLocation(locId);
                }));
        }
    }

    // Stop navigation, shown while there's a route to cancel. hasRoute()
    // rather than isNavigating() so the entry stays put through Rerouting
    // and Arrived, which still hold a route but aren't the Navigating status.
    navNode->addChild(MenuNode::action(QStringLiteral("nav_stop"),
        tr->menuStopNavigation(), [this]() {
            if (m_navigationService) m_navigationService->clearNavigation();
            close();
        }, [this]() {
            return m_navigationService && m_navigationService->hasRoute();
        }));

    // Navigation setup info (always available for proactive offline downloads)
    navNode->addChild(MenuNode::action(QStringLiteral("nav_setup"), tr->menuNavSetup(), [this]() {
        closeForScreen();
        if (m_screenStore) m_screenStore->showNavigationSetup(2); // Both
    }));

    // === Set up Navigation (visible when routing is not ready) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("setup_navigation"),
        tr->menuSetupNavigation(), [this]() {
            closeForScreen();
            if (m_screenStore) m_screenStore->showNavigationSetup(1); // Routing
        }, [this]() {
            return !isRoutingReady();
        }));

    // === Set up Map Mode (only on cluster screen, when no local maps and not online) ===
    // Sits next to Set up Navigation: both are one-time setup prompts, and
    // keeping them together stops a setup entry from appearing between the
    // two view-switch entries.
    m_rootNode->addChild(MenuNode::action(QStringLiteral("setup_map_mode"),
        tr->menuSetupMapMode(), [this]() {
            closeForScreen();
            if (m_screenStore) m_screenStore->showNavigationSetup(0); // DisplayMaps
        }, [this]() {
            if (!m_screenStore || m_screenStore->currentScreen() != 0) return false;
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return !hasLocalMaps && !isOnlineMap;
        }));

    // === Switch to Cluster View (anywhere but the cluster) ===
    // "Not already there" rather than "on the map". Keyed on the map, this
    // entry disappeared on the debug screen, and Switch to Map was keyed on
    // the cluster so it was missing there too: the debug screen offered no
    // way back to a dashboard at all.
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_cluster"),
        tr->menuSwitchToCluster(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(0);
            m_settingsService->updateMode(QStringLiteral("speedometer"));
            close();
        }, [this]() {
            return m_screenStore && m_screenStore->currentScreen() != 0;
        }));

    // === Switch to Map View (anywhere but the map, requires local maps or online map type) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_map"),
        tr->menuSwitchToMap(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(1);
            m_settingsService->updateMode(QStringLiteral("navigation"));
            close();
        }, [this]() {
            if (!m_screenStore || m_screenStore->currentScreen() == 1) return false;
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return hasLocalMaps || isOnlineMap;
        }));

    // === Lock Scooter (top-level, only when strictly parked) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("lock_scooter"),
        tr->menuLockScooter(), [this, repo]() {
            repo->push(QStringLiteral("scooter:state"), QStringLiteral("lock"));
            close();
        }, [this]() {
            return m_vehicle &&
                   m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Parked);
        }));

    // === Toggle Hazard Lights (top-level, like Flutter) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("hazard_lights"),
        tr->menuToggleHazardLights(), [this, repo]() {
            // Toggle hazard lights via MDB (match Flutter logic using LPUSH)
            int state = m_vehicle->blinkerState();
            bool isBoth = state == static_cast<int>(ScootEnums::BlinkerState::Both);
            QString cmd = isBoth ? QStringLiteral("off") : QStringLiteral("both");
            qDebug() << "Toggle hazards: blinkerState=" << state << "isBoth=" << isBoth << "pushing=" << cmd;
            repo->push(QStringLiteral("scooter:blinker"), cmd);
            close();
        }));

    // Root-menu faults entry — only shown when at least one fault is active.
    if (m_faults && m_faults->activeCount() > 0) {
        const QString label = QStringLiteral("%1 (%2)")
                                .arg(tr->menuFaults())
                                .arg(m_faults->activeCount());
        m_rootNode->addChild(MenuNode::action(QStringLiteral("faults_root"), label, [this]() {
            closeForScreen();
            if (m_screenStore)
                m_screenStore->showFaults();
        }));
    }

    // === Settings submenu ===
    // No explicit header: MenuNode falls back to the title uppercased, which
    // is the same words already translated. Spelling the header out again as a
    // literal is how these four ended up stuck in English.
    auto *settingsNode = MenuNode::submenu(QStringLiteral("settings"),
                                           tr->menuSettings());
    m_rootNode->addChild(settingsNode);

    // Settings groups by topic. Ten flat entries did not fit the screen and
    // mixed cosmetic set-once toggles in with the features riders look for,
    // so everything hangs off four groups instead. The group nodes are
    // declared up front; each block below files itself under one of them.
    auto *appearanceNode = MenuNode::submenu(QStringLiteral("settings_appearance"),
                                             tr->menuAppearance(),
                                             tr->menuAppearance().toUpper());
    settingsNode->addChild(appearanceNode);

    auto *vehicleNode = MenuNode::submenu(QStringLiteral("settings_vehicle"),
                                          tr->menuVehicle(),
                                          tr->menuVehicle().toUpper());
    settingsNode->addChild(vehicleNode);

    // Theme (inline cycle: Auto → Dark → Light) — kept at top, used often.
    {
        int themeIdx = isAutoTheme ? 0 : (isDark ? 1 : 2);
        appearanceNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_theme"),
            tr->menuTheme(), {
                {tr->menuThemeAuto(), [svc]() { svc->updateAutoTheme(true); }},
                {tr->menuThemeDark(), [svc]() { svc->updateTheme(QStringLiteral("dark")); }},
                {tr->menuThemeLight(), [svc]() { svc->updateTheme(QStringLiteral("light")); }},
            }, themeIdx));
    }

    // Backlight (inline cycle: Auto -> Low -> Medium -> High). Auto = ambient
    // light sensor; the fixed levels override brightness only, not the theme.
    {
        const QString blMode = settings->backlightMode();
        int blIdx = 0; // auto
        if (blMode == QLatin1String("low")) blIdx = 1;
        else if (blMode == QLatin1String("medium")) blIdx = 2;
        else if (blMode == QLatin1String("high")) blIdx = 3;
        appearanceNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_backlight"),
            tr->menuBacklight(), {
                {tr->menuBacklightAuto(),   [svc]() { svc->updateBacklightMode(QStringLiteral("auto")); }},
                {tr->menuBacklightLow(),    [svc]() { svc->updateBacklightMode(QStringLiteral("low")); }},
                {tr->menuBacklightMedium(), [svc]() { svc->updateBacklightMode(QStringLiteral("medium")); }},
                {tr->menuBacklightHigh(),   [svc]() { svc->updateBacklightMode(QStringLiteral("high")); }},
            }, blIdx));
    }

    // Power Display (inline cycle: kW → Amps) — units for the cluster power bar.
    {
        int powerIdx = (settings->powerDisplayMode() == static_cast<int>(ScootEnums::PowerDisplayMode::Amps)) ? 1 : 0;
        appearanceNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_power_display"),
            tr->menuPowerDisplay(), {
                {tr->menuPowerDisplayKw(),   [svc]() { svc->updatePowerDisplayMode(QStringLiteral("kw")); }},
                {tr->menuPowerDisplayAmps(), [svc]() { svc->updatePowerDisplayMode(QStringLiteral("amps")); }},
            }, powerIdx));
    }

    // Hop-on — learning / disabling the combo. Promoted to near the top
    // since it's a discoverable feature riders will want to find.
    // First-run (no combo): opens the info screen so the rider understands
    // what's about to happen. 'Set new combo…' from the combo-present
    // submenu still jumps straight to the learning overlay.
    if (m_hopOn) {
        if (!m_hopOn->hasCombo()) {
            vehicleNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), [this]() {
                    closeForScreen();
                    if (m_screenStore)
                        m_screenStore->showHopOnInfo();
                }));
        } else {
            auto *hopNode = MenuNode::submenu(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), tr->menuHopOnHeader());
            vehicleNode->addChild(hopNode);

            hopNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on_relearn"),
                tr->menuHopOnRelearn(), [this]() {
                    close();
                    m_hopOn->startLearning();
                }));
            hopNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on_disable"),
                tr->menuHopOnDisable(), [this]() {
                    m_hopOn->disable();
                    close();
                }));
        }
    }

    // Status Bar (flat list of inline cycle settings)
    auto *statusBarNode = MenuNode::submenu(QStringLiteral("settings_status_bar"),
                                            tr->menuStatusBar());


    // Battery Display (inline cycle: Percentage → Range → Icons only)
    {
        QString battMode = settings->batteryDisplayMode();
        int battIdx = battMode == QLatin1String("range") ? 1
                    : (battMode == QLatin1String("icon") ? 2 : 0);
        statusBarNode->addChild(MenuNode::cycleSetting(QStringLiteral("status_battery"),
            tr->menuBatteryDisplay(), {
                {tr->menuBatteryPercentage(), [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("percentage")); }},
                {tr->menuBatteryRange(), [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("range")); }},
                {tr->menuBatteryIconsOnly(), [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("icon")); }},
            }, battIdx));
    }

    // Optional CBB / AUX charge indicators (icon-only): visibility cycle
    // always / warning (SoC <= 50%) / never, one per battery. Shares the
    // "warning" value with the temperature indicator; labelled "When Low".
    auto addBatteryVisibility = [&](const QString &id, const QString &title,
                                     const QString &currentVal,
                                     std::function<void(const QString&)> updateFn) {
        QString val = currentVal.isEmpty() ? QStringLiteral("warning") : currentVal;
        int idx = 0;
        if (val == QLatin1String("warning")) idx = 1;
        else if (val == QLatin1String("never")) idx = 2;
        statusBarNode->addChild(MenuNode::cycleSetting(id, title, {
            {tr->optAlways(), [updateFn]() { updateFn(QStringLiteral("always")); }},
            {tr->optWhenLow(), [updateFn]() { updateFn(QStringLiteral("warning")); }},
            {tr->optNever(), [updateFn]() { updateFn(QStringLiteral("never")); }},
        }, idx));
    };

    addBatteryVisibility(QStringLiteral("status_cb_battery"), tr->menuCbBattery(),
        settings->showCbBattery(), [svc](const QString &v) { svc->updateShowCbBattery(v); });
    addBatteryVisibility(QStringLiteral("status_aux_battery"), tr->menuAuxBattery(),
        settings->showAuxBattery(), [svc](const QString &v) { svc->updateShowAuxBattery(v); });

    // Helper for 4-option visibility cycle settings
    auto addVisibilityCycle = [&](const QString &id, const QString &title,
                                   const QString &currentVal, const QString &defaultVal,
                                   std::function<void(const QString&)> updateFn) {
        QString val = currentVal.isEmpty() ? defaultVal : currentVal;
        int idx = 0;
        if (val == QLatin1String("active-or-error")) idx = 1;
        else if (val == QLatin1String("error")) idx = 2;
        else if (val == QLatin1String("never")) idx = 3;
        statusBarNode->addChild(MenuNode::cycleSetting(id, title, {
            {tr->optAlways(), [updateFn]() { updateFn(QStringLiteral("always")); }},
            {tr->optActiveOrError(), [updateFn]() { updateFn(QStringLiteral("active-or-error")); }},
            {tr->optErrorOnly(), [updateFn]() { updateFn(QStringLiteral("error")); }},
            {tr->optNever(), [updateFn]() { updateFn(QStringLiteral("never")); }},
        }, idx));
    };

    // Read visibility settings from SettingsStore (already synced from Redis)
    addVisibilityCycle(QStringLiteral("status_gps"), tr->menuGpsIcon(),
        settings->showGps(), QStringLiteral("error"),
        [svc](const QString &v) { svc->updateShowGps(v); });
    addVisibilityCycle(QStringLiteral("status_bluetooth"), tr->menuBluetoothIcon(),
        settings->showBluetooth(), QStringLiteral("active-or-error"),
        [svc](const QString &v) { svc->updateShowBluetooth(v); });
    addVisibilityCycle(QStringLiteral("status_cloud"), tr->menuCloudIcon(),
        settings->showCloud(), QStringLiteral("never"),
        [svc](const QString &v) { svc->updateShowCloud(v); });
    addVisibilityCycle(QStringLiteral("status_internet"), tr->menuInternetIcon(),
        settings->showInternet(), QStringLiteral("never"),
        [svc](const QString &v) { svc->updateShowInternet(v); });

    // Clock (inline cycle: Time → Date + Time → Alternating → Never)
    // "always" is the time-only value: it predates the date formats and is
    // what existing vehicles have stored, so it keeps its meaning and only
    // its label changed.
    {
        QString clkVal = settings->showClock();
        if (clkVal.isEmpty()) clkVal = QStringLiteral("always");
        int clkIdx = 0;
        if (clkVal == QLatin1String("date-time")) clkIdx = 1;
        else if (clkVal == QLatin1String("alternate")) clkIdx = 2;
        else if (clkVal == QLatin1String("never")) clkIdx = 3;
        statusBarNode->addChild(MenuNode::cycleSetting(QStringLiteral("status_clock"),
            tr->menuClock(), {
                {tr->optTime(), [svc]() { svc->updateShowClock(QStringLiteral("always")); }},
                {tr->optDateTime(), [svc]() { svc->updateShowClock(QStringLiteral("date-time")); }},
                {tr->optAlternating(), [svc]() { svc->updateShowClock(QStringLiteral("alternate")); }},
                {tr->optNever(), [svc]() { svc->updateShowClock(QStringLiteral("never")); }},
            }, clkIdx));
    }

    // Temperature (inline cycle: Always → Warning → Never)
    {
        QString tempVal = settings->showTemperature();
        if (tempVal.isEmpty()) tempVal = QStringLiteral("warning");
        int tempIdx = 0;
        if (tempVal == QLatin1String("warning")) tempIdx = 1;
        else if (tempVal == QLatin1String("never")) tempIdx = 2;
        statusBarNode->addChild(MenuNode::cycleSetting(QStringLiteral("status_temperature"),
            tr->menuTemperature(), {
                {tr->optAlways(), [svc]() { svc->updateShowTemperature(QStringLiteral("always")); }},
                {tr->optWarningOnly(), [svc]() { svc->updateShowTemperature(QStringLiteral("warning")); }},
                {tr->optNever(), [svc]() { svc->updateShowTemperature(QStringLiteral("never")); }},
            }, tempIdx));
    }

    // Map & Navigation (flat list of inline cycle settings)
    auto *mapNavNode = MenuNode::submenu(QStringLiteral("settings_map"),
                                         tr->menuMapNav());
    settingsNode->addChild(mapNavNode);

    // Map View (inline cycle: 3D → 2D). 2D is a flat top-down camera.
    {
        int viewMode = settings->mapViewMode();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_view"),
            tr->menuMapView(), {
                {tr->menuView3d(), [svc]() { svc->updateMapViewMode(QStringLiteral("3d")); }},
                {tr->menuView2d(), [svc]() { svc->updateMapViewMode(QStringLiteral("2d")); }},
            }, viewMode == 1 ? 1 : 0));
    }

    // Map Orientation (inline cycle: Heading → North). Only relevant in the 2D
    // view; hidden in 3D where the camera always follows forward.
    {
        bool northOriented = settings->mapNorthOriented();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_orientation"),
            tr->menuOrientation(), {
                {tr->menuHeadingUp(), [svc]() { svc->updateMapNorthOriented(false); }},
                {tr->menuNorthOriented(), [svc]() { svc->updateMapNorthOriented(true); }},
            }, northOriented ? 1 : 0))
            ->setIsVisible([this]() {
                return m_settings->mapViewMode() == static_cast<int>(ScootEnums::MapViewMode::View2D);
            });
    }

    // Route Preference (inline cycle: Fastest → Shortest)
    {
        bool isShortest = settings->routePreference() == QLatin1String("shortest");
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("route_preference"),
            tr->menuRoutePreference(), {
                {tr->menuRouteFastest(),  [svc]() { svc->updateRoutePreference(QStringLiteral("fastest")); }},
                {tr->menuRouteShortest(), [svc]() { svc->updateRoutePreference(QStringLiteral("shortest")); }},
            }, isShortest ? 1 : 0));
    }

    // Avoid Cobblestone (inline cycle: Off → Low → Medium → High). Hidden while
    // the route preference is Shortest: that mode costs by raw distance, so the
    // surface weight would have no effect and the control would be lying.
    {
        const QString level = settings->avoidCobblestone();
        int cobbleIdx = 2; // medium
        if (level == QLatin1String("off")) cobbleIdx = 0;
        else if (level == QLatin1String("low")) cobbleIdx = 1;
        else if (level == QLatin1String("high")) cobbleIdx = 3;
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("avoid_cobblestone"),
            tr->menuAvoidCobblestone(), {
                {tr->optOff(),    [svc]() { svc->updateAvoidCobblestone(QStringLiteral("off")); }},
                {tr->optLow(),    [svc]() { svc->updateAvoidCobblestone(QStringLiteral("low")); }},
                {tr->optMedium(), [svc]() { svc->updateAvoidCobblestone(QStringLiteral("medium")); }},
                {tr->optHigh(),   [svc]() { svc->updateAvoidCobblestone(QStringLiteral("high")); }},
            }, cobbleIdx))
            ->setIsVisible([this]() {
                // Shortest costs by raw distance and never reaches the surface
                // weight. The public Valhalla runs stock tiles and stock
                // costing, so it cannot see sett either: hide rather than offer
                // a control that silently does nothing.
                return m_settings->routePreference() != QLatin1String("shortest")
                    && m_settings->valhallaUrl() != QLatin1String(AppConfig::valhallaOnlineEndpoint);
            });
    }

    // Map Updates (inline cycle: Off -> Notify -> Download). Two settings sit
    // behind this: checking and auto-downloading. Only three of their four
    // combinations mean anything, since a download cannot happen without a
    // check finding one, so one control writes both and the fourth state
    // stops existing.
    {
        const bool checkUpdates = settings->mapCheckForUpdates();
        const bool autoDownload = settings->mapAutoDownload();
        int updatesIdx = 0;
        if (checkUpdates && autoDownload) updatesIdx = 2;
        else if (checkUpdates) updatesIdx = 1;
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_updates"),
            tr->menuMapUpdates(), {
                {tr->optOff(), [svc]() {
                    svc->updateMapCheckForUpdates(false);
                    svc->updateMapAutoDownload(false);
                }},
                {tr->optNotify(), [svc]() {
                    svc->updateMapCheckForUpdates(true);
                    svc->updateMapAutoDownload(false);
                }},
                {tr->optDownload(), [svc]() {
                    svc->updateMapCheckForUpdates(true);
                    svc->updateMapAutoDownload(true);
                }},
            }, updatesIdx));
    }

    // Check for Updates Now. The automatic check runs at most weekly and only
    // once the modem reports connected, so this is the way to ask on demand,
    // and the only way at all while automatic updates are off.
    if (m_mapDownload) {
        auto *checkNode = MenuNode::action(QStringLiteral("map_check_now"),
            tr->menuMapCheckNow(), [this]() {
                if (!m_internet || m_internet->modemState()
                        != static_cast<int>(ScootEnums::ModemState::Connected)) {
                    if (m_toastService)
                        m_toastService->showError(m_translations->navSetupDownloadNoInternet());
                    close();
                    return;
                }

                // Report the outcome once, for this check only: the automatic
                // path stays silent when nothing changed.
                auto *conn = new QMetaObject::Connection;
                *conn = connect(m_mapDownload, &MapDownloadService::updateCheckCompleted,
                                this, [this, conn](bool updateFound) {
                    disconnect(*conn);
                    delete conn;
                    if (!m_toastService)
                        return;
                    if (updateFound)
                        m_toastService->showInfo(m_translations->mapUpdateAvailableToast());
                    else
                        m_toastService->showSuccess(m_translations->mapsUpToDateToast());
                });

                if (m_toastService)
                    m_toastService->showInfo(m_translations->mapCheckingToast());
                m_mapDownload->checkForUpdatesNow();
                close();
            },
            [this]() { return m_mapDownload && m_mapDownload->hasMapsInstalled(); });
        checkNode->setValueLabel(lastMapCheckLabel());
        mapNavNode->addChild(checkNode);
    }

    // Map Type (inline cycle: Online → Offline)
    {
        int mapType = settings->mapType();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_type"),
            tr->menuMapType(), {
                {tr->menuOnline(), [svc]() { svc->updateMapType(QStringLiteral("online")); }},
                {tr->menuOffline(), [svc]() { svc->updateMapType(QStringLiteral("offline")); }},
            }, mapType == 1 ? 1 : 0));
    }

    // Navigation Routing (inline cycle: Online → Offline)
    {
        QString vUrl = settings->valhallaUrl();
        bool isOnlineRouting = (vUrl == QLatin1String(AppConfig::valhallaOnlineEndpoint));
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("navigation_routing"),
            tr->menuNavRouting(), {
                {tr->menuOnline(), [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnlineEndpoint)); }},
                {tr->menuOffline(), [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnDeviceEndpoint)); }},
            }, isOnlineRouting ? 0 : 1));
    }

    // Blinkers: on-screen style plus the physical LED on the DBC board. Both
    // are about how the blinker is presented rather than how it behaves, so
    // they sit under Appearance as a single entry.
    {
        auto *blinkerNode = MenuNode::submenu(QStringLiteral("settings_blinker"),
                                              tr->menuBlinker(),
                                              tr->menuBlinkerHeader());
        appearanceNode->addChild(blinkerNode);

        // Blinker Style (inline cycle: Icon → Overlay)
        QString bStyle = settings->blinkerStyle();
        int blinkerIdx = (bStyle == QLatin1String("overlay")) ? 1 : 0;
        blinkerNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_blinker_style"),
            tr->menuBlinkerStyle(), {
                {tr->menuBlinkerIcon(), [svc]() { svc->updateBlinkerStyle(QStringLiteral("icon")); }},
                {tr->menuBlinkerOverlay(), [svc]() { svc->updateBlinkerStyle(QStringLiteral("overlay")); }},
            }, blinkerIdx));

        // DBC Blinker LED (toggle) — physical LED on the DBC board.
        bool dbcLedOn = settings->dbcBlinkerLed();
        blinkerNode->addChild(MenuNode::setting(QStringLiteral("settings_dbc_blinker_led"),
            tr->menuDbcBlinkerLed(), dbcLedOn ? 1 : 0,
            [svc, dbcLedOn]() { svc->updateDbcBlinkerLed(!dbcLedOn); }));
    }

    // Status Bar (nine visibility toggles, built further up). Filed directly
    // after Blinkers so the two submenus sit together instead of being split
    // by a toggle.
    appearanceNode->addChild(statusBarNode);

    // Milestone Celebrations (toggle): confetti + banner when passing a
    // 500 km milestone or an easter-egg number. Off by default and the least
    // consequential setting on the vehicle, so it goes last.
    {
        bool milestonesOn = settings->milestoneCelebrations();
        appearanceNode->addChild(MenuNode::setting(QStringLiteral("settings_milestones"),
            tr->menuMilestones(), milestonesOn ? 1 : 0,
            [svc, milestonesOn]() { svc->updateMilestoneCelebrations(!milestonesOn); }));
    }

    // Alarm
    auto *alarmNode = MenuNode::submenu(QStringLiteral("settings_alarm"), tr->menuAlarm());
    vehicleNode->addChild(alarmNode);
    bool alarmOn = settings->alarmEnabled();
    bool alarmHonkOn = settings->alarmHonk();
    QString alarmDur = settings->alarmDuration();

    alarmNode->addChild(MenuNode::setting(QStringLiteral("alarm_enabled"), tr->menuAlarmEnable(),
        alarmOn ? 1 : 0, [svc, alarmOn]() { svc->updateAlarmEnabled(!alarmOn); }));
    alarmNode->addChild(MenuNode::setting(QStringLiteral("alarm_honk"), tr->menuAlarmHonk(),
        alarmHonkOn ? 1 : 0, [svc, alarmHonkOn]() { svc->updateAlarmHonk(!alarmHonkOn); }));

    // Alarm Duration (inline cycle: 10s → 20s → 30s)
    {
        int durIdx = 0;
        if (alarmDur == QLatin1String("20")) durIdx = 1;
        else if (alarmDur == QLatin1String("30")) durIdx = 2;
        alarmNode->addChild(MenuNode::cycleSetting(QStringLiteral("alarm_duration"),
            tr->menuAlarmDuration(), {
                {tr->menuAlarmDuration10(), [svc]() { svc->updateAlarmDuration(10); }},
                {tr->menuAlarmDuration20(), [svc]() { svc->updateAlarmDuration(20); }},
                {tr->menuAlarmDuration30(), [svc]() { svc->updateAlarmDuration(30); }},
            }, durIdx));
    }

    // System — rarely-touched knobs (Language, Battery Mode) live here
    // alongside the service entries (Update Mode, Faults).
    auto *systemNode = MenuNode::submenu(QStringLiteral("settings_system"), tr->menuSystem());
    settingsNode->addChild(systemNode);

    // Language (inline cycle: English → Deutsch)
    {
        int langIdx = (currentLang == QLatin1String("de")) ? 1 : 0;
        systemNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_language"),
            tr->menuLanguage(), {
                {QStringLiteral("English"), [svc]() { svc->updateLanguage(QStringLiteral("en")); }},
                {QStringLiteral("Deutsch"), [svc]() { svc->updateLanguage(QStringLiteral("de")); }},
            }, langIdx));
    }

    // Battery Mode (inline cycle: Single → Dual) — set-once on install.
    {
        bool dualBatt = settings->dualBattery();
        vehicleNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_battery_mode"),
            tr->menuBatteryMode(), {
                {tr->menuBatterySingle(), [svc]() { svc->updateDualBattery(false); }},
                {tr->menuBatteryDual(), [svc]() { svc->updateDualBattery(true); }},
            }, dualBatt ? 1 : 0));
    }

    // Horn While Seatbox Open (toggle) — off mutes the manual horn when the
    // open seat lid rests on the button. Default off; on restores legacy honk.
    {
        bool hornSeatboxOpen = settings->hornWhenSeatboxOpen();
        vehicleNode->addChild(MenuNode::setting(QStringLiteral("settings_horn_seatbox_open"),
            tr->menuHornSeatboxOpen(), hornSeatboxOpen ? 1 : 0,
            [svc, hornSeatboxOpen]() { svc->updateHornWhenSeatboxOpen(!hornSeatboxOpen); }));
    }

    systemNode->addChild(MenuNode::action(QStringLiteral("enter_ums"), tr->menuEnterUms(), [this]() {
        closeForScreen();
        if (m_screenStore)
            m_screenStore->showUpdateModeInfo();
    }));

    systemNode->addChild(MenuNode::action(QStringLiteral("enable_service_mode"),
        tr->menuServiceMode(), [this, repo]() {
            repo->push(QStringLiteral("settings:overlay"), QStringLiteral("apply:service"));
            close();
        }, [this]() {
            return !(m_settings && m_settings->serviceActive() == QLatin1String("true"));
        }));

    // Updates — ordered by how often a rider has any business touching them:
    // how often to look, look now, and then the two that the defaults already
    // get right. All four write the MDB and DBC keys together (see
    // SettingsService::writeOtaSetting): the two boards ship as a pair.
    if (m_updateChannel) {
        auto *updatesNode = systemNode->addChild(MenuNode::submenu(
            QStringLiteral("settings_updates"), tr->menuUpdates(), tr->menuUpdatesHeader()));
        // The only row in here a rider taps more than once in the vehicle's
        // life. The other three are set once and left.
        updatesNode->setPrimaryChildId(QStringLiteral("settings_update_check_now"));

        // Every entry below writes the MDB and DBC keys together, so the two
        // normally agree and one value speaks for both. lsc or a hand-edited
        // settings.toml can move one without the other, and then no single
        // value is the setting: say so and leave it at that. Which board is
        // where is a question for lsc, not for a menu whose only offer is one
        // channel on both. Nothing is marked as current in that state either,
        // so picking any option writes the pair again and heals it.
        const QString divergedLabel = tr->menuDiverged();

        // Check Frequency. "0" disables scheduled checks entirely; the manual
        // entry below is then the only way an update is ever found. Nothing
        // below 6h is offered: releases do not land more often than that, and
        // a shorter interval only spends cellular data to learn nothing.
        {
            struct Interval { QString label; QString value; };
            const Interval intervals[] = {
                {tr->updateFreqOff(),  QStringLiteral("0")},
                {QStringLiteral("6h"),  QStringLiteral("6h")},
                {QStringLiteral("12h"), QStringLiteral("12h")},
                {QStringLiteral("24h"), QStringLiteral("24h")},
                {QStringLiteral("3d"),  QStringLiteral("72h")},
                {QStringLiteral("7d"),  QStringLiteral("168h")},
            };
            const bool freqSplit = settings->otaCheckIntervalDiverged();
            const QString currentInterval = freqSplit ? QString() : settings->otaCheckInterval();

            QList<CycleOption> options;
            int index = -1;
            for (int i = 0; i < int(std::size(intervals)); ++i) {
                const QString value = intervals[i].value;
                options.append({intervals[i].label, [svc, value]() { svc->updateOtaCheckInterval(value); }});
                if (value == currentInterval)
                    index = i;
            }

            auto *freqNode = updatesNode->addChild(MenuNode::cycleSetting(
                QStringLiteral("settings_update_frequency"), tr->menuUpdateFrequency(),
                options, index < 0 ? 0 : index));
            // An interval set outside this menu (lsc, settings.toml) need not be
            // one of the six offered here. Show what it actually is rather than
            // silently mislabelling it as the option the cycle happens to sit on.
            if (freqSplit) {
                freqNode->setValueLabel(divergedLabel);
                freqNode->setCaution(true);
            } else if (index < 0 && !currentInterval.isEmpty()) {
                freqNode->setValueLabel(currentInterval);
            }
        }

        // Check for Updates Now. The only way to look while the scheduled
        // check is switched off, and the way to pick up a release that landed
        // between checks.
        {
            auto *checkNode = MenuNode::action(QStringLiteral("settings_update_check_now"),
                tr->menuUpdateCheckNow(), [this]() {
                    if (m_toastService)
                        m_toastService->showInfo(m_translations->updateCheckStartedToast());
                    m_settingsService->triggerUpdateCheck();
                    close();
                });
            checkNode->setValueLabel(lastCheckLabel(settings->otaLastCheck()));
            updatesNode->addChild(checkNode);
        }

        // Update Type. Delta transfers only what changed between two releases
        // and needs the current image's artifact on disk to patch against;
        // Full always fetches the whole image.
        //
        // A submenu of checkable rows rather than an inline cycle, for the same
        // reason Release Channel is one: a cycle applies on the tap that lands
        // on it, and the tap that flips delta to full turns the next update
        // from a ~40 MB patch into a ~400 MB image over a metered SIM. The
        // current value still rides on the parent row so the list says which
        // one is set without being entered.
        {
            const bool typeSplit = settings->otaMethodDiverged();
            const bool full = settings->otaMethod() == QLatin1String("full");
            auto *typeNode = updatesNode->addChild(MenuNode::submenu(
                QStringLiteral("settings_update_type"), tr->menuUpdateType(),
                tr->menuUpdateTypeHeader()));
            auto methodLabel = [tr](const QString &v) {
                return v == QLatin1String("full") ? tr->menuUpdateTypeFull()
                                                  : tr->menuUpdateTypeDelta();
            };
            typeNode->setValueLabel(typeSplit ? divergedLabel
                                              : methodLabel(settings->otaMethod()));
            typeNode->setCaution(true);

            struct Choice { const char *id; QString label; QString value; };
            const Choice choices[] = {
                {"update_type_delta", tr->menuUpdateTypeDelta(), QStringLiteral("delta")},
                {"update_type_full",  tr->menuUpdateTypeFull(),  QStringLiteral("full")},
            };
            const QString current = typeSplit ? QString()
                                  : (full ? QStringLiteral("full") : QStringLiteral("delta"));
            for (const auto &c : choices) {
                const QString value = c.value;
                const bool isCurrent = (value == current);
                typeNode->addChild(MenuNode::setting(QLatin1String(c.id), c.label,
                    isCurrent ? 1 : 0, [this, svc, value, isCurrent]() {
                        if (!isCurrent)
                            svc->updateOtaMethod(value);
                        goBack();
                    }));
            }
        }

        // Release Channel. A submenu of checkable rows rather than an inline
        // cycle: cycling would commit each channel it passed through, and
        // every commit is a full-image download.
        {
            auto *channelNode = updatesNode->addChild(MenuNode::submenu(
                QStringLiteral("settings_update_channel"), tr->menuUpdateChannel(),
                tr->menuUpdateChannelHeader()));
            channelNode->setCaution(true);

            const bool channelSplit = settings->otaChannelDiverged();
            const QString current = channelSplit ? QString()
                                                 : m_updateChannel->currentChannel();
            struct Choice { const char *id; QString label; QString value; };
            const Choice choices[] = {
                {"update_channel_stable", tr->channelStable(), QStringLiteral("stable")},
                {"update_channel_testing", tr->channelTesting(), QStringLiteral("testing")},
                {"update_channel_nightly", tr->channelNightly(), QStringLiteral("nightly")},
            };
            // Same list drives the row's trailing label, so the label and the
            // checkmark can never disagree about which channel is set.
            if (channelSplit) {
                channelNode->setValueLabel(divergedLabel);
            } else {
                for (const auto &c : choices) {
                    if (c.value == current)
                        channelNode->setValueLabel(c.label);
                }
            }
            for (const auto &c : choices) {
                const QString value = c.value;
                const bool isCurrent = (value == current);
                channelNode->addChild(MenuNode::setting(QLatin1String(c.id), c.label,
                    isCurrent ? 1 : 0, [this, value, isCurrent]() {
                        if (isCurrent) {
                            goBack();
                            return;
                        }
                        if (m_updateChannel->isUpdateInProgress()) {
                            if (m_toastService)
                                m_toastService->showError(m_translations->updateCheckBusyToast());
                            close();
                            return;
                        }
                        m_updateChannel->beginSwitch(value);
                        closeForScreen();
                        if (m_screenStore)
                            m_screenStore->showUpdateChannel();
                    }));
            }
        }
    }

    // Capture Logs — `ssh mdb lsc logs`. Closes the menu immediately and
    // runs the bundle job in the background. A toast confirms it kicked off,
    // then a second toast reports success or failure when ssh exits. Assumes
    // DBC->MDB key-based ssh is set up by the image build; bundle lands in
    // /data/logs-<timestamp>.tar.gz on MDB.
    systemNode->addChild(MenuNode::action(QStringLiteral("capture_logs"),
                                          tr->menuCaptureLogs(), [this]() {
        auto *proc = new QProcess(this);
        proc->setProgram(QStringLiteral("ssh"));
        proc->setArguments({QStringLiteral("-y"), QStringLiteral("mdb"),
                            QStringLiteral("lsc"), QStringLiteral("logs")});

        connect(proc, &QProcess::errorOccurred, this,
                [this, proc](QProcess::ProcessError err) {
            qWarning() << "[MenuStore] Capture Logs ssh errorOccurred:" << err;
            if (m_toastService)
                m_toastService->showError(m_translations->captureLogsToastFailed());
            proc->deleteLater();
        });

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc](int exitCode, QProcess::ExitStatus status) {
            qInfo() << "[MenuStore] Capture Logs ssh finished, exitCode:" << exitCode
                    << "status:" << status;
            if (m_toastService) {
                if (status == QProcess::NormalExit && exitCode == 0)
                    m_toastService->showSuccess(m_translations->captureLogsToastDone());
                else
                    m_toastService->showError(m_translations->captureLogsToastFailed());
            }
            proc->deleteLater();
        });

        proc->start();
        qInfo() << "[MenuStore] Capture Logs triggered";
        if (m_toastService)
            m_toastService->showInfo(m_translations->captureLogsToastStarted());
        close();
    }));

    // === Info (root level) ===
    //
    // Read-only diagnostics, not settings, and the thing support and
    // connectivity onboarding point people at. Under Settings > System it took
    // five screens to reach an IMEI; from the root it takes three. About moves
    // in with it, being the same kind of read-only page, which keeps the root
    // menu the same length as before.
    {
        const int activeFaults = m_faults ? m_faults->activeCount() : 0;
        auto withCount = [activeFaults](const QString &label) {
            return activeFaults > 0 ? QStringLiteral("%1 (%2)").arg(label).arg(activeFaults)
                                    : label;
        };

        auto *infoNode = MenuNode::submenu(QStringLiteral("info"), withCount(tr->menuInfo()),
                                           tr->menuInfo().toUpper());
        m_rootNode->addChild(infoNode);

        struct Page { const char *id; QString title; int page; };
        const Page pages[] = {
            {"info_components", tr->menuInfoComponents(), ScreenStore::SystemInfoDevice},
            {"info_connectivity", tr->menuInfoConnectivity(), ScreenStore::SystemInfoConnectivity},
            {"info_batteries", tr->menuInfoBatteries(), ScreenStore::SystemInfoBatteries},
            {"info_maps", tr->menuInfoMaps(), ScreenStore::SystemInfoMaps},
        };
        for (const auto &p : pages) {
            const int page = p.page;
            infoNode->addChild(MenuNode::action(QLatin1String(p.id), p.title, [this, page]() {
                closeForScreen();
                if (m_screenStore)
                    m_screenStore->showSystemInfo(page);
            }));
        }

        infoNode->addChild(MenuNode::action(QStringLiteral("faults"),
                                            withCount(tr->menuFaults()), [this]() {
            closeForScreen();
            if (m_screenStore)
                m_screenStore->showFaults();
        }));

        infoNode->addChild(MenuNode::action(QStringLiteral("about"), tr->menuAbout(), [this]() {
            closeForScreen();
            if (m_screenStore)
                m_screenStore->showAbout();
        }));
    }

    m_rootNode->addChild(MenuNode::action(QStringLiteral("exit"), tr->menuExit(), [this]() {
        close();
    }));

    // Restore path if possible
    m_pathStack.clear();
    m_indexStack.clear();
    MenuNode *current = m_rootNode.get();
    for (int i = 0; i < savedPath.size(); ++i) {
        bool found = false;
        for (auto *child : current->visibleChildren()) {
            if (child->id() == savedPath[i]) {
                m_pathStack.append(savedPath[i]);
                m_indexStack.append(savedIndexStack[i]);
                current = child;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    // Restore onto the row the selection was last known to be on, by id.
    // m_selectedId was recorded while the list still had its old shape, which
    // is the whole point: an entry appearing or disappearing above it shifts
    // every row below, and cycling Navigation Routing to Offline reveals Avoid
    // Cobblestone higher up the same list.
    const auto restoredChildren = current->visibleChildren();
    int selected = -1;
    if (!m_selectedId.isEmpty()) {
        for (int i = 0; i < restoredChildren.size(); ++i) {
            if (restoredChildren[i]->id() == m_selectedId) {
                selected = i;
                break;
            }
        }
    }
    // The row itself can be the one that went away, in which case its old
    // position is the closest thing to where the rider was looking.
    m_selectedIndex = selected >= 0
                    ? selected
                    : qBound(0, savedIndex, qMax(0, (int)restoredChildren.size() - 1));
    rememberSelection();

    emitMenuChanged();
}

void MenuStore::rememberSelection()
{
    m_selectedId.clear();
    if (MenuNode *node = findCurrentNode()) {
        const auto children = node->visibleChildren();
        if (m_selectedIndex >= 0 && m_selectedIndex < children.size())
            m_selectedId = children[m_selectedIndex]->id();
    }
}

MenuNode *MenuStore::findCurrentNode() const
{
    if (!m_rootNode) return nullptr;

    MenuNode *node = m_rootNode.get();
    for (const auto &id : m_pathStack) {
        bool found = false;
        for (auto *child : node->visibleChildren()) {
            if (child->id() == id) {
                node = child;
                found = true;
                break;
            }
        }
        if (!found) return m_rootNode.get();
    }
    return node;
}

// The level a Back lands on, for the hold hint. Empty at the root, where the
// hold leaves the menu instead of going up.
QString MenuStore::parentTitle() const
{
    if (m_pathStack.isEmpty() || !m_rootNode)
        return {};
    if (m_pathStack.size() == 1)
        return m_translations->menuMainMenu();

    MenuNode *node = m_rootNode.get();
    for (int i = 0; i < m_pathStack.size() - 1; ++i) {
        MenuNode *next = nullptr;
        for (auto *child : node->visibleChildren()) {
            if (child->id() == m_pathStack[i]) {
                next = child;
                break;
            }
        }
        if (!next)
            return {};
        node = next;
    }
    return node->title();
}

// The row a right long-tap would act on: the selected row's declared primary
// child, looked up among the children it would show if entered. Returns
// nothing when the row declares none, or when the child it names is hidden by
// its own predicate.
MenuNode *MenuStore::selectedPrimaryNode() const
{
    MenuNode *node = findCurrentNode();
    if (!node)
        return nullptr;

    const auto rows = node->visibleChildren();
    if (m_selectedIndex < 0 || m_selectedIndex >= rows.size())
        return nullptr;

    const QString primaryId = rows[m_selectedIndex]->primaryChildId();
    if (primaryId.isEmpty())
        return nullptr;

    for (auto *child : rows[m_selectedIndex]->visibleChildren()) {
        if (child->id() == primaryId)
            return child;
    }
    return nullptr;
}

QString MenuStore::selectedPrimaryLabel() const
{
    MenuNode *primary = selectedPrimaryNode();
    return primary ? primary->title() : QString();
}

void MenuStore::activatePrimary()
{
    MenuNode *primary = selectedPrimaryNode();
    if (!primary)
        return;

    // Same guard selectItem() takes round an action: starting a route or
    // touching the saved list signals a rebuild that would destroy the tree
    // this call is standing in.
    auto action = primary->action();
    m_executingAction = true;
    if (action) action();
    m_executingAction = false;
    rebuildMenuTree();
}

QString MenuStore::currentTitle() const
{
    auto *node = findCurrentNode();
    return node ? node->headerTitle() : m_translations->menuTitle();
}

QVariantList MenuStore::currentItems() const
{
    auto *node = findCurrentNode();
    if (!node) return {};

    QVariantList list;

    // No synthetic back row: the bottom bar names the hold that goes back, so
    // a row that does the same thing would cost every submenu a line.
    for (auto *child : node->visibleChildren()) {
        QVariantMap item;
        item[QStringLiteral("id")] = child->id();
        item[QStringLiteral("title")] = child->title();
        item[QStringLiteral("type")] = child->type() == MenuNodeType::Action ? QStringLiteral("action")
                                     : child->type() == MenuNodeType::Submenu ? QStringLiteral("submenu")
                                     : child->type() == MenuNodeType::CycleSetting ? QStringLiteral("cycle")
                                     : QStringLiteral("setting");
        item[QStringLiteral("currentValue")] = child->currentValue();
        item[QStringLiteral("hasChildren")] = child->hasChildren();
        if (child->type() == MenuNodeType::CycleSetting || !child->currentValueLabel().isEmpty())
            item[QStringLiteral("valueLabel")] = child->currentValueLabel();
        if (child->caution())
            item[QStringLiteral("caution")] = true;
        if (child->id() == QLatin1String("nav_setup")
            && m_mapDownload && m_mapDownload->updateAvailable())
            item[QStringLiteral("leadingIcon")] = QStringLiteral("\ue692"); // update
        list.append(item);
    }
    return list;
}

bool MenuStore::canScrollUp() const
{
    auto *node = findCurrentNode();
    if (!node) return false;
    int totalCount = node->visibleChildren().size();
    return totalCount > 1;
}

bool MenuStore::canScrollDown() const
{
    auto *node = findCurrentNode();
    if (!node) return false;
    int totalCount = node->visibleChildren().size();
    return totalCount > 1;
}

void MenuStore::toggle()
{
    if (m_isOpen)
        close();
    else
        open();
}
void MenuStore::open()
{
    openAt({}, {}, 0);
}

void MenuStore::openAt(const QStringList &path, const QList<int> &indexStack, int index)
{
    qDebug() << "MenuStore: open requested, vehicleState" << m_vehicle->state()
             << "isOpen" << m_isOpen << "hopOnMode" << (m_hopOn ? m_hopOn->mode() : -1)
             << "path" << path;

    if (!m_vehicle->isParked()) {
        qDebug() << "MenuStore: open dropped - not parked, vehicleState" << m_vehicle->state();
        return;
    }
    if (m_isOpen) {
        qDebug() << "MenuStore: open dropped - already open";
        return;
    }
    if (m_hopOn && m_hopOn->mode() != HopOnStore::Idle) {
        qDebug() << "MenuStore: open dropped - hop-on not idle, mode" << m_hopOn->mode();
        return;
    }

    qDebug() << "MenuStore: opening menu";
    clearResume();
    m_isOpen = true;
    // rebuildMenuTree() replays the path against the tree it just built and
    // stops at the first level that no longer exists, so a stale path lands
    // on the nearest surviving ancestor rather than nowhere.
    m_pathStack = path;
    m_indexStack = indexStack;
    m_selectedIndex = index;
    m_openedAt.start();
    rebuildMenuTree();
    if (m_repo) {
        m_repo->set(QStringLiteral("dashboard"),
                    QStringLiteral("menu-open"),
                    QStringLiteral("true"));
    }
    emit isOpenChanged();
}

void MenuStore::closeForScreen()
{
    const QStringList path = m_pathStack;
    const QList<int> indexStack = m_indexStack;
    const int index = m_selectedIndex;
    close();
    m_resumePath = path;
    m_resumeIndexStack = indexStack;
    m_resumeIndex = index;
    m_resumeArmed = true;
}

void MenuStore::resume()
{
    if (!m_resumeArmed) return;
    const QStringList path = m_resumePath;
    const QList<int> indexStack = m_resumeIndexStack;
    const int index = m_resumeIndex;
    clearResume();
    openAt(path, indexStack, index);
}

void MenuStore::clearResume()
{
    m_resumeArmed = false;
    m_resumePath.clear();
    m_resumeIndexStack.clear();
    m_resumeIndex = 0;
}

void MenuStore::close()
{
    clearResume();
    if (!m_isOpen) return;
    m_isOpen = false;
    m_selectedIndex = 0;
    m_pathStack.clear();
    m_indexStack.clear();
    if (m_repo) {
        m_repo->set(QStringLiteral("dashboard"),
                    QStringLiteral("menu-open"),
                    QStringLiteral("false"));
    }
    emit isOpenChanged();
    emitMenuChanged();
}

void MenuStore::navigateUp()
{
    if (m_openedAt.isValid() && m_openedAt.elapsed() < kOpenInputGraceMs) return;
    auto *node = findCurrentNode();
    if (!node) return;
    int totalCount = node->visibleChildren().size();
    if (totalCount <= 1) return;
    m_selectedIndex = (m_selectedIndex - 1 + totalCount) % totalCount;
    rememberSelection();
    emitMenuChanged();
}

void MenuStore::navigateDown()
{
    if (m_openedAt.isValid() && m_openedAt.elapsed() < kOpenInputGraceMs) return;
    auto *node = findCurrentNode();
    if (!node) return;
    int totalCount = node->visibleChildren().size();
    if (totalCount <= 1) return;
    m_selectedIndex = (m_selectedIndex + 1) % totalCount;
    rememberSelection();
    emitMenuChanged();
}

void MenuStore::selectItem()
{
    auto *node = findCurrentNode();
    if (!node) return;

    auto children = node->visibleChildren();
    if (m_selectedIndex < 0 || m_selectedIndex >= children.size()) return;

    auto *selected = children[m_selectedIndex];

    if (selected->type() == MenuNodeType::CycleSetting) {
        // Inline cycle: advance to next option and apply it
        m_executingAction = true;
        selected->cycleNext();
        m_executingAction = false;
        rebuildMenuTree();
    } else if (selected->type() == MenuNodeType::Submenu && selected->hasChildren()) {
        // Enter submenu
        m_pathStack.append(selected->id());
        m_indexStack.append(m_selectedIndex);
        m_selectedIndex = 0;
        rememberSelection();
        emitMenuChanged();
    } else {
        // Guard: prevent signal-triggered rebuildMenuTree() during action
        // execution. Actions like navigateToLocation() trigger load() →
        // locationsChanged → rebuildMenuTree(), which would destroy the
        // menu tree out from under us. The guard defers rebuilds until
        // after the action completes.
        auto action = selected->action();
        m_executingAction = true;
        if (action) action();
        m_executingAction = false;
        rebuildMenuTree();
    }
}

void MenuStore::goBack()
{
    if (m_pathStack.isEmpty()) {
        close();
    } else {
        const QString leaving = m_pathStack.takeLast();
        m_selectedIndex = m_indexStack.isEmpty() ? 0 : m_indexStack.takeLast();
        // Land back on the row that was entered, found by id. The level may
        // have gained or lost entries while the rider was inside it, and the
        // stored index would then point at whatever slid into that slot.
        if (MenuNode *node = findCurrentNode()) {
            const auto children = node->visibleChildren();
            for (int i = 0; i < children.size(); ++i) {
                if (children[i]->id() == leaving) {
                    m_selectedIndex = i;
                    break;
                }
            }
            m_selectedIndex = qBound(0, m_selectedIndex, qMax(0, (int)children.size() - 1));
        }
        rememberSelection();
        emitMenuChanged();
    }
}

void MenuStore::emitMenuChanged()
{
    emit menuChanged();
}

bool MenuStore::isRoutingReady() const
{
    // Routing is ready if local valhalla responds OR scooter is online with online routing configured
    if (m_navAvailability && m_navAvailability->routingAvailable())
        return true;
    bool isOnline = m_internet &&
        m_internet->modemState() == static_cast<int>(ScootEnums::ModemState::Connected);
    bool isOnlineRouting = m_settings &&
        m_settings->valhallaUrl() == QLatin1String(AppConfig::valhallaOnlineEndpoint);
    return isOnline && isOnlineRouting;
}
