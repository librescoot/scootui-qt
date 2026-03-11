#include "MenuStore.h"
#include "models/MenuNode.h"
#include "SettingsStore.h"
#include "VehicleStore.h"
#include "ThemeStore.h"
#include "TripStore.h"
#include "ScreenStore.h"
#include "SavedLocationsStore.h"
#include "l10n/Translations.h"
#include "services/SettingsService.h"
#include "services/NavigationService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

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
    connect(m_translations, &Translations::languageChanged, this, &MenuStore::rebuildMenuTree);

    // Close menu when vehicle starts moving
    connect(m_vehicle, &VehicleStore::stateChanged, this, [this]() {
        if (m_isOpen && !m_vehicle->isParked()) {
            close();
        }
    });

    rebuildMenuTree();
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

void MenuStore::rebuildMenuTree()
{
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
            // Toggle hazard lights via MDB
            repo->set(QStringLiteral("vehicle"), QStringLiteral("blinker"),
                      QStringLiteral("both"));
            close();
        }));

    // === Switch to Cluster View (only on map screen) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_cluster"),
        tr->menuSwitchToCluster(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(0);
            close();
        }, [this]() {
            return m_screenStore && m_screenStore->currentScreen() == 1;
        }));

    // === Switch to Map View (only on cluster screen) ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("switch_map"),
        tr->menuSwitchToMap(), [this]() {
            if (m_screenStore) m_screenStore->setScreen(1);
            close();
        }, [this]() {
            return m_screenStore && m_screenStore->currentScreen() == 0;
        }));

    // === Navigation submenu ===
    // Use header for display title; the submenu list title is just "Navigation"
    auto *navNode = MenuNode::submenu(QStringLiteral("navigation"),
                                       QStringLiteral("Navigation"),
                                       tr->menuNavigationHeader());
    m_rootNode->addChild(navNode);

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

            savedLocsNode->addChild(MenuNode::action(
                QStringLiteral("saved_loc_%1").arg(locId), label,
                [this, locId]() {
                    m_savedLocations->navigateToLocation(locId);
                    close();
                }));
        }
    }

    // Stop navigation
    navNode->addChild(MenuNode::action(QStringLiteral("nav_stop"),
        tr->menuStopNavigation(), [this]() {
            if (m_navigationService) m_navigationService->clearNavigation();
            close();
        }));

    // Navigation setup info
    navNode->addChild(MenuNode::action(QStringLiteral("nav_setup"), tr->menuNavSetup(), [this]() {
        close();
        if (m_screenStore) m_screenStore->setScreen(9);
    }));

    // === Settings submenu ===
    auto *settingsNode = MenuNode::submenu(QStringLiteral("settings"),
                                           tr->menuSettings(),
                                           QStringLiteral("SETTINGS"));
    m_rootNode->addChild(settingsNode);

    // Theme
    auto *themeNode = MenuNode::submenu(QStringLiteral("settings_theme"),
                                        tr->menuTheme(),
                                        tr->menuTheme().toUpper());
    settingsNode->addChild(themeNode);

    themeNode->addChild(MenuNode::setting(QStringLiteral("theme_auto"), tr->menuThemeAuto(),
        isAutoTheme ? 1 : 0, [svc]() { svc->updateAutoTheme(true); }));
    themeNode->addChild(MenuNode::setting(QStringLiteral("theme_dark"), tr->menuThemeDark(),
        (!isAutoTheme && isDark) ? 1 : 0, [svc]() { svc->updateTheme(QStringLiteral("dark")); }));
    themeNode->addChild(MenuNode::setting(QStringLiteral("theme_light"), tr->menuThemeLight(),
        (!isAutoTheme && !isDark) ? 1 : 0, [svc]() { svc->updateTheme(QStringLiteral("light")); }));

    // Language
    auto *langNode = MenuNode::submenu(QStringLiteral("settings_language"),
                                       tr->menuLanguage(),
                                       tr->menuLanguage().toUpper());
    settingsNode->addChild(langNode);

    langNode->addChild(MenuNode::setting(QStringLiteral("lang_en"), QStringLiteral("English"),
        currentLang == QLatin1String("en") ? 1 : 0, [svc]() { svc->updateLanguage(QStringLiteral("en")); }));
    langNode->addChild(MenuNode::setting(QStringLiteral("lang_de"), QStringLiteral("Deutsch"),
        currentLang == QLatin1String("de") ? 1 : 0, [svc]() { svc->updateLanguage(QStringLiteral("de")); }));

    // Status Bar
    auto *statusBarNode = MenuNode::submenu(QStringLiteral("settings_status_bar"),
                                            tr->menuStatusBar());
    settingsNode->addChild(statusBarNode);

    // Battery Display
    auto *battDispNode = MenuNode::submenu(QStringLiteral("status_battery"),
                                           tr->menuBatteryDisplay());
    statusBarNode->addChild(battDispNode);
    QString battMode = settings->batteryDisplayMode();
    battDispNode->addChild(MenuNode::setting(QStringLiteral("battery_percentage"), tr->menuBatteryPercentage(),
        battMode != QLatin1String("range") ? 1 : 0, [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("percentage")); }));
    battDispNode->addChild(MenuNode::setting(QStringLiteral("battery_range"), tr->menuBatteryRange(),
        battMode == QLatin1String("range") ? 1 : 0, [svc]() { svc->updateBatteryDisplayMode(QStringLiteral("range")); }));

    // Helper for 4-option visibility submenus
    auto addVisibilitySubmenu = [&](const QString &id, const QString &title,
                                    const QString &currentVal, const QString &defaultVal,
                                    std::function<void(const QString&)> updateFn) {
        auto *node = MenuNode::submenu(id, title);
        statusBarNode->addChild(node);
        QString val = currentVal.isEmpty() ? defaultVal : currentVal;
        node->addChild(MenuNode::setting(id + QStringLiteral("_always"), tr->optAlways(),
            val == QLatin1String("always") ? 1 : 0, [updateFn]() { updateFn(QStringLiteral("always")); }));
        node->addChild(MenuNode::setting(id + QStringLiteral("_active"), tr->optActiveOrError(),
            val == QLatin1String("active-or-error") ? 1 : 0, [updateFn]() { updateFn(QStringLiteral("active-or-error")); }));
        node->addChild(MenuNode::setting(id + QStringLiteral("_error"), tr->optErrorOnly(),
            val == QLatin1String("error") ? 1 : 0, [updateFn]() { updateFn(QStringLiteral("error")); }));
        node->addChild(MenuNode::setting(id + QStringLiteral("_never"), tr->optNever(),
            val == QLatin1String("never") ? 1 : 0, [updateFn]() { updateFn(QStringLiteral("never")); }));
    };

    // Read raw string values for visibility settings
    auto gpsVal = repo->get(QStringLiteral("settings"), QStringLiteral("dashboard.show-gps"));
    auto btVal = repo->get(QStringLiteral("settings"), QStringLiteral("dashboard.show-bluetooth"));
    auto cloudVal = repo->get(QStringLiteral("settings"), QStringLiteral("dashboard.show-cloud"));
    auto inetVal = repo->get(QStringLiteral("settings"), QStringLiteral("dashboard.show-internet"));
    auto clockVal = repo->get(QStringLiteral("settings"), QStringLiteral("dashboard.show-clock"));

    addVisibilitySubmenu(QStringLiteral("status_gps"), tr->menuGpsIcon(), gpsVal, QStringLiteral("error"),
        [svc](const QString &v) { svc->updateShowGps(v); });
    addVisibilitySubmenu(QStringLiteral("status_bluetooth"), tr->menuBluetoothIcon(), btVal, QStringLiteral("active-or-error"),
        [svc](const QString &v) { svc->updateShowBluetooth(v); });
    addVisibilitySubmenu(QStringLiteral("status_cloud"), tr->menuCloudIcon(), cloudVal, QStringLiteral("error"),
        [svc](const QString &v) { svc->updateShowCloud(v); });
    addVisibilitySubmenu(QStringLiteral("status_internet"), tr->menuInternetIcon(), inetVal, QStringLiteral("always"),
        [svc](const QString &v) { svc->updateShowInternet(v); });

    // Clock (2 options only)
    auto *clockNode = MenuNode::submenu(QStringLiteral("status_clock"), tr->menuClock());
    statusBarNode->addChild(clockNode);
    QString clkVal = clockVal.isEmpty() ? QStringLiteral("always") : clockVal;
    clockNode->addChild(MenuNode::setting(QStringLiteral("clock_always"), tr->optAlways(),
        clkVal != QLatin1String("never") ? 1 : 0, [svc]() { svc->updateShowClock(QStringLiteral("always")); }));
    clockNode->addChild(MenuNode::setting(QStringLiteral("clock_never"), tr->optNever(),
        clkVal == QLatin1String("never") ? 1 : 0, [svc]() { svc->updateShowClock(QStringLiteral("never")); }));

    // Map & Navigation
    auto *mapNavNode = MenuNode::submenu(QStringLiteral("settings_map"), tr->menuMapNav());
    settingsNode->addChild(mapNavNode);

    auto *renderNode = MenuNode::submenu(QStringLiteral("map_render_mode"), tr->menuRenderMode());
    mapNavNode->addChild(renderNode);
    int renderMode = settings->mapRenderMode();
    renderNode->addChild(MenuNode::setting(QStringLiteral("render_vector"), tr->menuVector(),
        renderMode == 0 ? 1 : 0, [svc]() { svc->updateMapRenderMode(QStringLiteral("vector")); }));
    renderNode->addChild(MenuNode::setting(QStringLiteral("render_raster"), tr->menuRaster(),
        renderMode == 1 ? 1 : 0, [svc]() { svc->updateMapRenderMode(QStringLiteral("raster")); }));

    auto *mapTypeNode = MenuNode::submenu(QStringLiteral("map_type"), tr->menuMapType());
    mapNavNode->addChild(mapTypeNode);
    int mapType = settings->mapType();
    mapTypeNode->addChild(MenuNode::setting(QStringLiteral("map_online"), tr->menuOnline(),
        mapType == 0 ? 1 : 0, [svc]() { svc->updateMapType(QStringLiteral("online")); }));
    mapTypeNode->addChild(MenuNode::setting(QStringLiteral("map_offline"), tr->menuOffline(),
        mapType == 1 ? 1 : 0, [svc]() { svc->updateMapType(QStringLiteral("offline")); }));

    auto *routingNode = MenuNode::submenu(QStringLiteral("navigation_routing"), tr->menuNavRouting());
    mapNavNode->addChild(routingNode);
    QString vUrl = settings->valhallaUrl();
    bool isOnlineRouting = (vUrl == QLatin1String(AppConfig::valhallaOnlineEndpoint));
    routingNode->addChild(MenuNode::setting(QStringLiteral("routing_online"), tr->menuOnlineOsm(),
        isOnlineRouting ? 1 : 0, [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnlineEndpoint)); }));
    routingNode->addChild(MenuNode::setting(QStringLiteral("routing_offline"), tr->menuOffline(),
        !isOnlineRouting ? 1 : 0, [svc]() { svc->updateValhallaEndpoint(QLatin1String(AppConfig::valhallaOnDeviceEndpoint)); }));

    // Blinker Style
    auto *blinkerNode = MenuNode::submenu(QStringLiteral("settings_blinker_style"), tr->menuBlinkerStyle());
    settingsNode->addChild(blinkerNode);
    QString bStyle = settings->blinkerStyle();
    blinkerNode->addChild(MenuNode::setting(QStringLiteral("blinker_icon"), tr->menuBlinkerIcon(),
        bStyle != QLatin1String("overlay") ? 1 : 0, [svc]() { svc->updateBlinkerStyle(QStringLiteral("icon")); }));
    blinkerNode->addChild(MenuNode::setting(QStringLiteral("blinker_overlay"), tr->menuBlinkerOverlay(),
        bStyle == QLatin1String("overlay") ? 1 : 0, [svc]() { svc->updateBlinkerStyle(QStringLiteral("overlay")); }));

    // Battery Mode
    auto *battModeNode = MenuNode::submenu(QStringLiteral("settings_battery_mode"), tr->menuBatteryMode());
    settingsNode->addChild(battModeNode);
    bool dualBatt = settings->dualBattery();
    battModeNode->addChild(MenuNode::setting(QStringLiteral("battery_mode_single"), tr->menuBatterySingle(),
        !dualBatt ? 1 : 0, [svc]() { svc->updateDualBattery(false); }));
    battModeNode->addChild(MenuNode::setting(QStringLiteral("battery_mode_dual"), tr->menuBatteryDual(),
        dualBatt ? 1 : 0, [svc]() { svc->updateDualBattery(true); }));

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

    auto *alarmDurNode = MenuNode::submenu(QStringLiteral("alarm_duration"), tr->menuAlarmDuration(),
                                           tr->menuAlarmDuration().toUpper());
    alarmNode->addChild(alarmDurNode);
    alarmDurNode->addChild(MenuNode::setting(QStringLiteral("alarm_dur_10"), tr->menuAlarmDuration10(),
        (alarmDur.isEmpty() || alarmDur == QLatin1String("10")) ? 1 : 0, [svc]() { svc->updateAlarmDuration(10); }));
    alarmDurNode->addChild(MenuNode::setting(QStringLiteral("alarm_dur_20"), tr->menuAlarmDuration20(),
        alarmDur == QLatin1String("20") ? 1 : 0, [svc]() { svc->updateAlarmDuration(20); }));
    alarmDurNode->addChild(MenuNode::setting(QStringLiteral("alarm_dur_30"), tr->menuAlarmDuration30(),
        alarmDur == QLatin1String("30") ? 1 : 0, [svc]() { svc->updateAlarmDuration(30); }));

    // System
    auto *systemNode = MenuNode::submenu(QStringLiteral("settings_system"), tr->menuSystem());
    settingsNode->addChild(systemNode);
    systemNode->addChild(MenuNode::action(QStringLiteral("enter_ums"), tr->menuEnterUms(), [this, repo]() {
        repo->set(QStringLiteral("usb"), QStringLiteral("mode"), QStringLiteral("ums-by-dbc"));
        close();
    }));

    // === Top-level actions ===
    m_rootNode->addChild(MenuNode::action(QStringLiteral("reset_trip"), tr->menuResetTrip(), [this, trip]() {
        trip->reset();
        close();
    }));

    m_rootNode->addChild(MenuNode::action(QStringLiteral("about"), tr->menuAbout(), [this]() {
        close();
        if (m_screenStore) {
            m_screenStore->setScreen(4); // About screen
        }
    }));

    m_rootNode->addChild(MenuNode::action(QStringLiteral("exit"), tr->menuExit(), [this]() {
        close();
    }));

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
        backItem[QStringLiteral("title")] = QStringLiteral("\u2190 ") + m_translations->controlBack();
        backItem[QStringLiteral("type")] = QStringLiteral("action");
        backItem[QStringLiteral("currentValue")] = 0;
        backItem[QStringLiteral("hasChildren")] = false;
        list.append(backItem);
    }

    for (auto *child : node->visibleChildren()) {
        QVariantMap item;
        item[QStringLiteral("id")] = child->id();
        item[QStringLiteral("title")] = child->title();
        item[QStringLiteral("type")] = child->type() == MenuNodeType::Action ? QStringLiteral("action")
                                     : child->type() == MenuNodeType::Submenu ? QStringLiteral("submenu")
                                     : QStringLiteral("setting");
        item[QStringLiteral("currentValue")] = child->currentValue();
        item[QStringLiteral("hasChildren")] = child->hasChildren();
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
    if (!m_vehicle->isParked()) return;
    if (m_isOpen) return;

    rebuildMenuTree();
    m_selectedIndex = 0;
    m_pathStack.clear();
    m_indexStack.clear();
    m_isOpen = true;
    emit isOpenChanged();
    emitMenuChanged();
}

void MenuStore::close()
{
    if (!m_isOpen) return;
    m_isOpen = false;
    m_selectedIndex = 0;
    m_pathStack.clear();
    m_indexStack.clear();
    emit isOpenChanged();
    emitMenuChanged();
}

void MenuStore::navigateUp()
{
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

    if (selected->type() == MenuNodeType::Submenu && selected->hasChildren()) {
        // Enter submenu
        m_pathStack.append(selected->id());
        m_indexStack.append(m_selectedIndex);
        m_selectedIndex = 0;
        emitMenuChanged();
    } else {
        // Execute action or setting
        selected->executeAction();
        // Rebuild to reflect changes (settings may have changed)
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
