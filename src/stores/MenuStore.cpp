#include "MenuStore.h"
#include "models/MenuNode.h"
#include "SettingsStore.h"
#include "VehicleStore.h"
#include "ThemeStore.h"
#include "TripStore.h"
#include "ScreenStore.h"
#include "SavedLocationsStore.h"
#include "InternetStore.h"
#include "HopOnStore.h"
#include "FaultsStore.h"
#include "l10n/Translations.h"
#include "services/SettingsService.h"
#include "services/NavigationService.h"
#include "services/NavigationAvailabilityService.h"
#include "services/MapDownloadService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>

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
    connect(m_settings, &SettingsStore::batteryDisplayModeChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmEnabledChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmHonkChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::alarmDurationChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showGpsChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showBluetoothChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showCloudChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showInternetChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::showClockChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapCheckForUpdatesChanged, this, &MenuStore::rebuildMenuTree);
    connect(m_settings, &SettingsStore::mapAutoDownloadChanged, this, &MenuStore::rebuildMenuTree);
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
    auto *trip = m_trip;
    auto *repo = m_repo;

    bool isAutoTheme = settings->theme() == QLatin1String("auto");
    bool isDark = m_theme->isDark();
    QString currentLang = settings->language();

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

    // === Hop-on activate (top-level, only when a combo is configured) ===
    if (m_hopOn && m_hopOn->hasCombo()) {
        m_rootNode->addChild(MenuNode::action(QStringLiteral("hop_on_activate"),
            tr->menuHopOnActivateTop(), [this]() {
                close();
                m_hopOn->activate();
            }));
    }

    // === Switch to Cluster View (only on map screen) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_cluster"),
        tr->menuSwitchToCluster(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(0);
            m_settingsService->updateMode(QStringLiteral("speedometer"));
            close();
        }, [this]() {
            return m_screenStore && m_screenStore->currentScreen() == 1;
        }));

    // === Switch to Map View (only on cluster screen, requires local maps or online map type) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_map"),
        tr->menuSwitchToMap(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(1);
            m_settingsService->updateMode(QStringLiteral("navigation"));
            close();
        }, [this]() {
            if (!m_screenStore || m_screenStore->currentScreen() != 0) return false;
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return hasLocalMaps || isOnlineMap;
        }));

    // === Set up Map Mode (only on cluster screen, when no local maps and not online) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("setup_map_mode"),
        tr->menuSetupMapMode(), [this]() {
            close();
            if (m_screenStore) m_screenStore->showNavigationSetup(0); // DisplayMaps
        }, [this]() {
            if (!m_screenStore || m_screenStore->currentScreen() != 0) return false;
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return !hasLocalMaps && !isOnlineMap;
        }));

    // === Navigation submenu (visible when display maps and routing are ready) ===
    auto *navNode = MenuNode::submenu(QStringLiteral("navigation"),
                                       QStringLiteral("Navigation"),
                                       tr->menuNavigationHeader(),
                                       [this]() {
            bool hasLocalMaps = m_navAvailability && m_navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = m_settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            bool routingReady = isRoutingReady();
            return (hasLocalMaps || isOnlineMap) && routingReady;
        });
    m_rootNode->addChild(navNode);

    // === Set up Navigation (visible when routing is not ready) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("setup_navigation"),
        tr->menuSetupNavigation(), [this]() {
            close();
            if (m_screenStore) m_screenStore->showNavigationSetup(1); // Routing
        }, [this]() {
            return !isRoutingReady();
        }));

    // Enter destination code
    navNode->addChild(MenuNode::action(QStringLiteral("nav_enter_code"),
        tr->menuEnterDestinationCode(), [this]() {
            close();
            if (m_screenStore) m_screenStore->setScreen(7); // AddressSelection
        }));

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

    // Stop navigation
    navNode->addChild(MenuNode::action(QStringLiteral("nav_stop"),
        tr->menuStopNavigation(), [this]() {
            if (m_navigationService) m_navigationService->clearNavigation();
            close();
        }));

    // Navigation setup info (always available for proactive offline downloads)
    navNode->addChild(MenuNode::action(QStringLiteral("nav_setup"), tr->menuNavSetup(), [this]() {
        close();
        if (m_screenStore) m_screenStore->showNavigationSetup(2); // Both
    }));

    // === Settings submenu ===
    auto *settingsNode = MenuNode::submenu(QStringLiteral("settings"),
                                           tr->menuSettings(),
                                           QStringLiteral("SETTINGS"));
    m_rootNode->addChild(settingsNode);

    // Theme (inline cycle: Auto → Dark → Light)
    {
        int themeIdx = isAutoTheme ? 0 : (isDark ? 1 : 2);
        settingsNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_theme"),
            tr->menuTheme(), {
                {tr->menuThemeAuto(), [svc]() { svc->updateAutoTheme(true); }},
                {tr->menuThemeDark(), [svc]() { svc->updateTheme(QStringLiteral("dark")); }},
                {tr->menuThemeLight(), [svc]() { svc->updateTheme(QStringLiteral("light")); }},
            }, themeIdx));
    }

    // Language (inline cycle: English → Deutsch)
    {
        int langIdx = (currentLang == QLatin1String("de")) ? 1 : 0;
        settingsNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_language"),
            tr->menuLanguage(), {
                {QStringLiteral("English"), [svc]() { svc->updateLanguage(QStringLiteral("en")); }},
                {QStringLiteral("Deutsch"), [svc]() { svc->updateLanguage(QStringLiteral("de")); }},
            }, langIdx));
    }

    // Status Bar (flat list of inline cycle settings)
    auto *statusBarNode = MenuNode::submenu(QStringLiteral("settings_status_bar"),
                                            tr->menuStatusBar(),
                                            QStringLiteral("STATUS BAR"));
    settingsNode->addChild(statusBarNode);

    // Battery Display (inline cycle: Percentage → Range)
    {
        QString battMode = settings->batteryDisplayMode();
        int battIdx = (battMode == QLatin1String("range")) ? 1 : 0;
        statusBarNode->addChild(MenuNode::cycleSetting(QStringLiteral("status_battery"),
            tr->menuBatteryDisplay(), {
                {tr->menuBatteryPercentage(), [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("percentage")); }},
                {tr->menuBatteryRange(), [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("range")); }},
            }, battIdx));
    }

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
        settings->showCloud(), QStringLiteral("error"),
        [svc](const QString &v) { svc->updateShowCloud(v); });
    addVisibilityCycle(QStringLiteral("status_internet"), tr->menuInternetIcon(),
        settings->showInternet(), QStringLiteral("always"),
        [svc](const QString &v) { svc->updateShowInternet(v); });

    // Clock (inline cycle: Always → Never)
    {
        QString clkVal = settings->showClock();
        if (clkVal.isEmpty()) clkVal = QStringLiteral("always");
        int clkIdx = (clkVal == QLatin1String("never")) ? 1 : 0;
        statusBarNode->addChild(MenuNode::cycleSetting(QStringLiteral("status_clock"),
            tr->menuClock(), {
                {tr->optAlways(), [svc]() { svc->updateShowClock(QStringLiteral("always")); }},
                {tr->optNever(), [svc]() { svc->updateShowClock(QStringLiteral("never")); }},
            }, clkIdx));
    }

    // Map & Navigation (flat list of inline cycle settings)
    auto *mapNavNode = MenuNode::submenu(QStringLiteral("settings_map"),
                                         tr->menuMapNav(),
                                         QStringLiteral("MAP"));
    settingsNode->addChild(mapNavNode);

    // Map Type (inline cycle: Online → Offline)
    {
        int mapType = settings->mapType();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_type"),
            tr->menuMapType(), {
                {tr->menuOnline(), [svc]() { svc->updateMapType(QStringLiteral("online")); }},
                {tr->menuOffline(), [svc]() { svc->updateMapType(QStringLiteral("offline")); }},
            }, mapType == 1 ? 1 : 0));
    }

    // Navigation Routing (inline cycle: Online OSM → Offline)
    {
        QString vUrl = settings->valhallaUrl();
        bool isOnlineRouting = (vUrl == QLatin1String(AppConfig::valhallaOnlineEndpoint));
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("navigation_routing"),
            tr->menuNavRouting(), {
                {tr->menuOnlineOsm(), [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnlineEndpoint)); }},
                {tr->menuOffline(), [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnDeviceEndpoint)); }},
            }, isOnlineRouting ? 0 : 1));
    }

    // Map Update Check (toggle)
    {
        bool checkUpdates = settings->mapCheckForUpdates();
        mapNavNode->addChild(MenuNode::setting(QStringLiteral("map_check_updates"),
            tr->menuMapUpdateCheck(), checkUpdates ? 1 : 0,
            [svc, checkUpdates]() { svc->updateMapCheckForUpdates(!checkUpdates); }));
    }

    // Auto-download Map Updates (toggle)
    {
        bool autoDownload = settings->mapAutoDownload();
        mapNavNode->addChild(MenuNode::setting(QStringLiteral("map_auto_download"),
            tr->menuMapAutoDownload(), autoDownload ? 1 : 0,
            [svc, autoDownload]() { svc->updateMapAutoDownload(!autoDownload); }));
    }

    // Blinker Style (inline cycle: Icon → Overlay)
    {
        QString bStyle = settings->blinkerStyle();
        int blinkerIdx = (bStyle == QLatin1String("overlay")) ? 1 : 0;
        settingsNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_blinker_style"),
            tr->menuBlinkerStyle(), {
                {tr->menuBlinkerIcon(), [svc]() { svc->updateBlinkerStyle(QStringLiteral("icon")); }},
                {tr->menuBlinkerOverlay(), [svc]() { svc->updateBlinkerStyle(QStringLiteral("overlay")); }},
            }, blinkerIdx));
    }

    // Battery Mode (inline cycle: Single → Dual)
    {
        bool dualBatt = settings->dualBattery();
        settingsNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_battery_mode"),
            tr->menuBatteryMode(), {
                {tr->menuBatterySingle(), [svc]() { svc->updateDualBattery(false); }},
                {tr->menuBatteryDual(), [svc]() { svc->updateDualBattery(true); }},
            }, dualBatt ? 1 : 0));
    }

    // Alarm
    auto *alarmNode = MenuNode::submenu(QStringLiteral("settings_alarm"), tr->menuAlarm(),
                                        QStringLiteral("ALARM"));
    settingsNode->addChild(alarmNode);
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

    // Hop-on — learning / disabling the combo
    if (m_hopOn) {
        if (!m_hopOn->hasCombo()) {
            settingsNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), [this]() {
                    close();
                    m_hopOn->startLearning();
                }));
        } else {
            auto *hopNode = MenuNode::submenu(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), tr->menuHopOnHeader());
            settingsNode->addChild(hopNode);

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

    // System
    auto *systemNode = MenuNode::submenu(QStringLiteral("settings_system"), tr->menuSystem());
    settingsNode->addChild(systemNode);
    systemNode->addChild(MenuNode::action(QStringLiteral("enter_ums"), tr->menuEnterUms(), [this, repo]() {
        repo->set(QStringLiteral("usb"), QStringLiteral("mode"), QStringLiteral("ums-by-dbc"));
        close();
    }));

    // Faults entry under settings — always visible, shows "(N)" when active > 0.
    {
        const int activeFaults = m_faults ? m_faults->activeCount() : 0;
        QString label = tr->menuFaults();
        if (activeFaults > 0)
            label = QStringLiteral("%1 (%2)").arg(label).arg(activeFaults);
        systemNode->addChild(MenuNode::action(QStringLiteral("faults"), label, [this]() {
            close();
            if (m_screenStore)
                m_screenStore->showFaults();
        }));
    }

    // === Top-level actions ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("reset_trip"), tr->menuResetTrip(), [this, trip]() {
        trip->reset();
        close();
    }));

    m_rootNode->addChild(MenuNode::action(QStringLiteral("about"), tr->menuAbout(), [this]() {
        close();
        if (m_screenStore) {
            m_screenStore->showAbout();
        }
    }));

    // Root-menu faults entry — only shown when at least one fault is active.
    if (m_faults && m_faults->activeCount() > 0) {
        const QString label = QStringLiteral("%1 (%2)")
                                .arg(tr->menuFaults())
                                .arg(m_faults->activeCount());
        m_rootNode->addChild(MenuNode::action(QStringLiteral("faults_root"), label, [this]() {
            close();
            if (m_screenStore)
                m_screenStore->showFaults();
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
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;
    m_selectedIndex = qMin(savedIndex, backOffset + (int)current->visibleChildren().size() - 1);

    emitMenuChanged();
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

QString MenuStore::currentTitle() const
{
    auto *node = findCurrentNode();
    return node ? node->headerTitle() : QStringLiteral("MENU");
}

QVariantList MenuStore::currentItems() const
{
    auto *node = findCurrentNode();
    if (!node) return {};

    QVariantList list;

    // Prepend back item only in submenus (Flutter doesn't have back/exit at root)
    if (!m_pathStack.isEmpty()) {
        QVariantMap backItem;
        backItem[QStringLiteral("id")] = QStringLiteral("__back");
        backItem[QStringLiteral("title")] = m_translations->controlBack();
        backItem[QStringLiteral("type")] = QStringLiteral("action");
        backItem[QStringLiteral("currentValue")] = 0;
        backItem[QStringLiteral("hasChildren")] = false;
        backItem[QStringLiteral("leadingIcon")] = QStringLiteral("\ue15e"); // chevron_left
        list.append(backItem);
    }

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
        if (child->type() == MenuNodeType::CycleSetting)
            item[QStringLiteral("valueLabel")] = child->currentValueLabel();
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
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;
    int totalCount = node->visibleChildren().size() + backOffset;
    return totalCount > 1;
}

bool MenuStore::canScrollDown() const
{
    auto *node = findCurrentNode();
    if (!node) return false;
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;
    int totalCount = node->visibleChildren().size() + backOffset;
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
    qDebug() << "MenuStore: open requested, vehicleState" << m_vehicle->state()
             << "isOpen" << m_isOpen << "hopOnMode" << (m_hopOn ? m_hopOn->mode() : -1);

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
    m_isOpen = true;
    m_pathStack.clear();
    m_indexStack.clear();
    m_selectedIndex = 0;
    m_openedAt.start();
    rebuildMenuTree();
    if (m_repo) {
        m_repo->set(QStringLiteral("dashboard"),
                    QStringLiteral("menu-open"),
                    QStringLiteral("true"));
    }
    emit isOpenChanged();
}

void MenuStore::close()
{
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
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;
    int totalCount = node->visibleChildren().size() + backOffset;
    if (totalCount <= 1) return;
    m_selectedIndex = (m_selectedIndex - 1 + totalCount) % totalCount;
    emitMenuChanged();
}

void MenuStore::navigateDown()
{
    if (m_openedAt.isValid() && m_openedAt.elapsed() < kOpenInputGraceMs) return;
    auto *node = findCurrentNode();
    if (!node) return;
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;
    int totalCount = node->visibleChildren().size() + backOffset;
    if (totalCount <= 1) return;
    m_selectedIndex = (m_selectedIndex + 1) % totalCount;
    emitMenuChanged();
}

void MenuStore::selectItem()
{
    int backOffset = m_pathStack.isEmpty() ? 0 : 1;

    // In submenus, index 0 is the back item
    if (backOffset > 0 && m_selectedIndex == 0) {
        goBack();
        return;
    }

    auto *node = findCurrentNode();
    if (!node) return;

    auto children = node->visibleChildren();
    int childIndex = m_selectedIndex - backOffset;
    if (childIndex < 0 || childIndex >= children.size()) return;

    auto *selected = children[childIndex];

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
        m_pathStack.removeLast();
        m_selectedIndex = m_indexStack.isEmpty() ? 0 : m_indexStack.takeLast();
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
