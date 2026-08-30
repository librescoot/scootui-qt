#include "MenuDefinition.h"
#include "MenuNode.h"

#include "stores/SettingsStore.h"
#include "stores/VehicleStore.h"
#include "stores/ThemeStore.h"
#include "stores/SavedLocationsStore.h"
#include "stores/RecentDestinationsStore.h"
#include "stores/InternetStore.h"
#include "services/HopOnService.h"
#include "services/FaultsService.h"
#include "services/NavigationService.h"
#include "services/NavigationAvailabilityService.h"
#include "services/MapDownloadService.h"
#include "services/UpdateChannelService.h"
#include "l10n/Translations.h"
#include "core/AppConfig.h"
#include "core/Navigator.h"
#include "models/Enums.h"

#include <QDateTime>
#include <iterator>

using Verb = MenuAction::Verb;

namespace {

// Trailing "2h ago" style label for a check that last ran at iso (ISO-8601).
QString lastCheckLabel(const Translations *tr, const QString &iso)
{
    if (iso.isEmpty())
        return tr->mapCheckNever();

    const QDateTime last = QDateTime::fromString(iso, Qt::ISODate);
    if (!last.isValid())
        return tr->mapCheckNever();

    // A clock that is behind the last check (boots before NTP) would otherwise
    // render as a negative age, so treat anything in the future as just now.
    const qint64 mins = last.secsTo(QDateTime::currentDateTimeUtc()) / 60;
    if (mins < 1)
        return tr->mapCheckJustNow();

    QString age;
    if (mins < 60)
        age = QStringLiteral("%1min").arg(mins);
    else if (mins < 60 * 24)
        age = QStringLiteral("%1h").arg(mins / 60);
    else
        age = QStringLiteral("%1d").arg(mins / (60 * 24));
    return tr->mapCheckAgo().arg(age);
}

}

namespace MenuDefinition {

bool isRoutingReady(const MenuContext &ctx)
{
    if (ctx.navAvailability && ctx.navAvailability->routingAvailable())
        return true;
    bool isOnline = ctx.internet &&
        ctx.internet->modemState() == static_cast<int>(ScootEnums::ModemState::Connected);
    bool isOnlineRouting = ctx.settings &&
        ctx.settings->valhallaUrl() == QLatin1String(AppConfig::valhallaOnlineEndpoint);
    return isOnline && isOnlineRouting;
}

std::unique_ptr<MenuNode> buildMenuTree(const MenuContext &ctx)
{
    auto *tr = ctx.tr;
    auto *settings = ctx.settings;

    auto root = std::unique_ptr<MenuNode>(
        MenuNode::submenu(QStringLiteral("root"), tr->menuTitle(), tr->menuTitle()));

    bool isAutoTheme = settings->theme() == QLatin1String("auto");
    bool isDark = ctx.theme->isDark();
    QString currentLang = settings->language();

    // === Disable Service Mode (top-level, only when service mode is active) ===
    root->addChild(MenuNode::action(QStringLiteral("disable_service_mode"),
        tr->menuDisableServiceMode(), {Verb::DisableServiceMode},
        [ctx]() {
            return ctx.settings && ctx.settings->serviceActive() == QLatin1String("true");
        }));

    // === Hop-on activate (top-level, only when a combo is configured) ===
    if (ctx.hopOn && ctx.hopOn->hasCombo()) {
        root->addChild(MenuNode::action(QStringLiteral("hop_on_activate"),
            tr->menuHopOnActivateTop(), {Verb::HopOnActivate}));
    }

    // === Navigation submenu (visible when display maps and routing are ready) ===
    auto *navNode = MenuNode::submenu(QStringLiteral("navigation"),
                                       tr->menuNavigation(),
                                       tr->menuNavigationHeader(),
                                       [ctx]() {
            bool hasLocalMaps = ctx.navAvailability && ctx.navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = ctx.settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return (hasLocalMaps || isOnlineMap) && isRoutingReady(ctx);
        });
    root->addChild(navNode);

    // Enter destination code
    navNode->addChild(MenuNode::action(QStringLiteral("nav_enter_code"),
        tr->menuEnterDestinationCode(),
        {Verb::ShowScreen, QStringLiteral("address-selection")}));

    // Recent destinations submenu (nested under Navigation) — last 10
    // destinations the rider has navigated to. Each gets a sub-submenu
    // with Start Navigation / Save to favorites / Delete.
    if (ctx.recentDestinations && ctx.recentDestinations->count() > 0) {
        auto *recentNode = MenuNode::submenu(QStringLiteral("recent_destinations"),
                                              tr->menuRecentDestinations());
        navNode->addChild(recentNode);

        auto dests = ctx.recentDestinations->destinations();
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
                tr->menuStartNavigation(), {Verb::StartRecent, {}, destId}));

            destNode->addChild(MenuNode::action(
                QStringLiteral("save_recent_%1").arg(destId),
                tr->menuSaveToFavorites(), {Verb::SaveRecent, {}, destId}));

            destNode->addChild(MenuNode::action(
                QStringLiteral("delete_recent_%1").arg(destId),
                tr->menuDeleteLocation(), {Verb::DeleteRecent, {}, destId}));
        }
    }

    // Saved locations submenu (nested under Navigation)
    if (ctx.savedLocations) {
        auto *savedLocsNode = MenuNode::submenu(QStringLiteral("saved_locations"),
                                                 tr->menuSavedLocations());
        navNode->addChild(savedLocsNode);

        savedLocsNode->addChild(MenuNode::action(QStringLiteral("save_current_loc"),
            tr->menuSaveLocation(), {Verb::SaveCurrentLocation}));

        auto locs = ctx.savedLocations->locations();
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
                tr->menuStartNavigation(), {Verb::StartSaved, {}, locId}));

            locNode->addChild(MenuNode::action(
                QStringLiteral("delete_loc_%1").arg(locId),
                tr->menuDeleteLocation(), {Verb::DeleteSaved, {}, locId}));
        }
    }

    // Stop navigation, shown while there's a route to cancel. hasRoute()
    // rather than isNavigating() so the entry stays put through Rerouting
    // and Arrived, which still hold a route but aren't the Navigating status.
    navNode->addChild(MenuNode::action(QStringLiteral("nav_stop"),
        tr->menuStopNavigation(), {Verb::StopNavigation},
        [ctx]() {
            return ctx.navigationService && ctx.navigationService->hasRoute();
        }));

    // Navigation setup info (always available for proactive offline downloads)
    auto *navSetupNode = MenuNode::action(QStringLiteral("nav_setup"), tr->menuNavSetup(),
        {Verb::ShowNavigationSetup, {}, 2 /* Both */});
    if (ctx.mapDownload && ctx.mapDownload->updateAvailable())
        navSetupNode->setLeadingIcon(QStringLiteral("\ue692")); // update glyph
    navNode->addChild(navSetupNode);

    // === Set up Navigation (visible when routing is not ready) ===
    root->addChild(MenuNode::action(QStringLiteral("setup_navigation"),
        tr->menuSetupNavigation(), {Verb::ShowNavigationSetup, {}, 1 /* Routing */},
        [ctx]() { return !isRoutingReady(ctx); }));

    // === Set up Map Mode (only on cluster screen, when no local maps and not online) ===
    // Sits next to Set up Navigation: both are one-time setup prompts, and
    // keeping them together stops a setup entry from appearing between the
    // two view-switch entries.
    root->addChild(MenuNode::action(QStringLiteral("setup_map_mode"),
        tr->menuSetupMapMode(), {Verb::ShowNavigationSetup, {}, 0 /* DisplayMaps */},
        [ctx]() {
            if (!ctx.navigator
                || ctx.navigator->currentScreenMode() != ScootEnums::ScreenMode::Cluster) return false;
            bool hasLocalMaps = ctx.navAvailability && ctx.navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = ctx.settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return !hasLocalMaps && !isOnlineMap;
        }));

    // === Switch to Cluster View (anywhere but the cluster) ===
    // "Not already there" rather than "on the map". Keyed on the map, this
    // entry disappeared on the debug screen, and Switch to Map was keyed on
    // the cluster so it was missing there too: the debug screen offered no
    // way back to a dashboard at all.
    root->addChild(MenuNode::action(QStringLiteral("switch_cluster"),
        tr->menuSwitchToCluster(), {Verb::SwitchView, QStringLiteral("cluster")},
        [ctx]() {
            return ctx.navigator
                && ctx.navigator->currentScreenMode() != ScootEnums::ScreenMode::Cluster;
        }));

    // === Switch to Map View (anywhere but the map, requires local maps or online map type) ===
    root->addChild(MenuNode::action(QStringLiteral("switch_map"),
        tr->menuSwitchToMap(), {Verb::SwitchView, QStringLiteral("map")},
        [ctx]() {
            if (!ctx.navigator
                || ctx.navigator->currentScreenMode() == ScootEnums::ScreenMode::Map) return false;
            bool hasLocalMaps = ctx.navAvailability && ctx.navAvailability->localDisplayMapsAvailable();
            bool isOnlineMap = ctx.settings->mapType() == static_cast<int>(ScootEnums::MapType::Online);
            return hasLocalMaps || isOnlineMap;
        }));

    // === Lock Scooter (top-level, only when strictly parked) ===
    root->addChild(MenuNode::action(QStringLiteral("lock_scooter"),
        tr->menuLockScooter(), {Verb::LockVehicle},
        [ctx]() {
            return ctx.vehicle &&
                   ctx.vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Parked);
        }));

    // === Toggle Hazard Lights (top-level) ===
    root->addChild(MenuNode::action(QStringLiteral("hazard_lights"),
        tr->menuToggleHazardLights(), {Verb::ToggleHazards}));

    // Root-menu faults entry — only shown when at least one fault is active.
    if (ctx.faults && ctx.faults->activeCount() > 0) {
        const QString label = QStringLiteral("%1 (%2)")
                                .arg(tr->menuFaults())
                                .arg(ctx.faults->activeCount());
        root->addChild(MenuNode::action(QStringLiteral("faults_root"), label,
            {Verb::ShowScreen, QStringLiteral("faults")}));
    }

    // === Settings submenu ===
    // No explicit header: MenuNode falls back to the title uppercased, which
    // is the same words already translated. Spelling the header out again as a
    // literal is how these four ended up stuck in English.
    auto *settingsNode = MenuNode::submenu(QStringLiteral("settings"),
                                           tr->menuSettings());
    root->addChild(settingsNode);

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
                {tr->menuThemeAuto(),  {Verb::SetSetting, QStringLiteral("auto-theme"), true}},
                {tr->menuThemeDark(),  {Verb::SetSetting, QStringLiteral("theme"), QStringLiteral("dark")}},
                {tr->menuThemeLight(), {Verb::SetSetting, QStringLiteral("theme"), QStringLiteral("light")}},
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
                {tr->menuBacklightAuto(),   {Verb::SetSetting, QStringLiteral("backlight-mode"), QStringLiteral("auto")}},
                {tr->menuBacklightLow(),    {Verb::SetSetting, QStringLiteral("backlight-mode"), QStringLiteral("low")}},
                {tr->menuBacklightMedium(), {Verb::SetSetting, QStringLiteral("backlight-mode"), QStringLiteral("medium")}},
                {tr->menuBacklightHigh(),   {Verb::SetSetting, QStringLiteral("backlight-mode"), QStringLiteral("high")}},
            }, blIdx));
    }

    // Power Display (inline cycle: kW → Amps) — units for the cluster power bar.
    {
        int powerIdx = (settings->powerDisplayMode() == static_cast<int>(ScootEnums::PowerDisplayMode::Amps)) ? 1 : 0;
        appearanceNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_power_display"),
            tr->menuPowerDisplay(), {
                {tr->menuPowerDisplayKw(),   {Verb::SetSetting, QStringLiteral("power-display-mode"), QStringLiteral("kw")}},
                {tr->menuPowerDisplayAmps(), {Verb::SetSetting, QStringLiteral("power-display-mode"), QStringLiteral("amps")}},
            }, powerIdx));
    }

    // Hop-on — learning / disabling the combo. Promoted to near the top
    // since it's a discoverable feature riders will want to find.
    // First-run (no combo): opens the info screen so the rider understands
    // what's about to happen. 'Set new combo…' from the combo-present
    // submenu still jumps straight to the learning overlay.
    if (ctx.hopOn) {
        if (!ctx.hopOn->hasCombo()) {
            vehicleNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), {Verb::ShowScreen, QStringLiteral("hop-on-info")}));
        } else {
            auto *hopNode = MenuNode::submenu(QStringLiteral("settings_hop_on"),
                tr->menuHopOn(), tr->menuHopOnHeader());
            vehicleNode->addChild(hopNode);

            hopNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on_relearn"),
                tr->menuHopOnRelearn(), {Verb::HopOnRelearn}));
            hopNode->addChild(MenuNode::action(QStringLiteral("settings_hop_on_disable"),
                tr->menuHopOnDisable(), {Verb::HopOnDisable}));
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
                {tr->menuBatteryPercentage(), {Verb::SetSetting, QStringLiteral("battery-display-mode"), QStringLiteral("percentage")}},
                {tr->menuBatteryRange(),      {Verb::SetSetting, QStringLiteral("battery-display-mode"), QStringLiteral("range")}},
                {tr->menuBatteryIconsOnly(),  {Verb::SetSetting, QStringLiteral("battery-display-mode"), QStringLiteral("icon")}},
            }, battIdx));
    }

    // Optional CBB / AUX charge indicators (icon-only): visibility cycle
    // always / when-low / never, one per battery. Shares the "warning" value
    // with the temperature indicator; labelled "When Low".
    auto addBatteryVisibility = [&](const QString &id, const QString &title,
                                     const QString &settingKey, const QString &currentVal) {
        QString val = currentVal.isEmpty() ? QStringLiteral("warning") : currentVal;
        int idx = 0;
        if (val == QLatin1String("warning")) idx = 1;
        else if (val == QLatin1String("never")) idx = 2;
        statusBarNode->addChild(MenuNode::cycleSetting(id, title, {
            {tr->optAlways(),  {Verb::SetSetting, settingKey, QStringLiteral("always")}},
            {tr->optWhenLow(), {Verb::SetSetting, settingKey, QStringLiteral("warning")}},
            {tr->optNever(),   {Verb::SetSetting, settingKey, QStringLiteral("never")}},
        }, idx));
    };

    addBatteryVisibility(QStringLiteral("status_cb_battery"), tr->menuCbBattery(),
        QStringLiteral("show-cb-battery"), settings->showCbBattery());
    addBatteryVisibility(QStringLiteral("status_aux_battery"), tr->menuAuxBattery(),
        QStringLiteral("show-aux-battery"), settings->showAuxBattery());

    // Helper for 4-option visibility cycle settings
    auto addVisibilityCycle = [&](const QString &id, const QString &title,
                                   const QString &settingKey,
                                   const QString &currentVal, const QString &defaultVal) {
        QString val = currentVal.isEmpty() ? defaultVal : currentVal;
        int idx = 0;
        if (val == QLatin1String("active-or-error")) idx = 1;
        else if (val == QLatin1String("error")) idx = 2;
        else if (val == QLatin1String("never")) idx = 3;
        statusBarNode->addChild(MenuNode::cycleSetting(id, title, {
            {tr->optAlways(),        {Verb::SetSetting, settingKey, QStringLiteral("always")}},
            {tr->optActiveOrError(), {Verb::SetSetting, settingKey, QStringLiteral("active-or-error")}},
            {tr->optErrorOnly(),     {Verb::SetSetting, settingKey, QStringLiteral("error")}},
            {tr->optNever(),         {Verb::SetSetting, settingKey, QStringLiteral("never")}},
        }, idx));
    };

    // Read visibility settings from SettingsStore (already synced from Redis)
    addVisibilityCycle(QStringLiteral("status_gps"), tr->menuGpsIcon(),
        QStringLiteral("show-gps"), settings->showGps(), QStringLiteral("error"));
    addVisibilityCycle(QStringLiteral("status_bluetooth"), tr->menuBluetoothIcon(),
        QStringLiteral("show-bluetooth"), settings->showBluetooth(), QStringLiteral("active-or-error"));
    addVisibilityCycle(QStringLiteral("status_cloud"), tr->menuCloudIcon(),
        QStringLiteral("show-cloud"), settings->showCloud(), QStringLiteral("never"));
    addVisibilityCycle(QStringLiteral("status_internet"), tr->menuInternetIcon(),
        QStringLiteral("show-internet"), settings->showInternet(), QStringLiteral("never"));

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
                {tr->optTime(),        {Verb::SetSetting, QStringLiteral("show-clock"), QStringLiteral("always")}},
                {tr->optDateTime(),    {Verb::SetSetting, QStringLiteral("show-clock"), QStringLiteral("date-time")}},
                {tr->optAlternating(), {Verb::SetSetting, QStringLiteral("show-clock"), QStringLiteral("alternate")}},
                {tr->optNever(),       {Verb::SetSetting, QStringLiteral("show-clock"), QStringLiteral("never")}},
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
                {tr->optAlways(),      {Verb::SetSetting, QStringLiteral("show-temperature"), QStringLiteral("always")}},
                {tr->optWarningOnly(), {Verb::SetSetting, QStringLiteral("show-temperature"), QStringLiteral("warning")}},
                {tr->optNever(),       {Verb::SetSetting, QStringLiteral("show-temperature"), QStringLiteral("never")}},
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
                {tr->menuView3d(), {Verb::SetSetting, QStringLiteral("map-view-mode"), QStringLiteral("3d")}},
                {tr->menuView2d(), {Verb::SetSetting, QStringLiteral("map-view-mode"), QStringLiteral("2d")}},
            }, viewMode == 1 ? 1 : 0));
    }

    // Map Orientation (inline cycle: Heading → North). Only relevant in the 2D
    // view; hidden in 3D where the camera always follows forward.
    {
        bool northOriented = settings->mapNorthOriented();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_orientation"),
            tr->menuOrientation(), {
                {tr->menuHeadingUp(),     {Verb::SetSetting, QStringLiteral("map-north-oriented"), false}},
                {tr->menuNorthOriented(), {Verb::SetSetting, QStringLiteral("map-north-oriented"), true}},
            }, northOriented ? 1 : 0))
            ->setIsVisible([ctx]() {
                return ctx.settings->mapViewMode() == static_cast<int>(ScootEnums::MapViewMode::View2D);
            });
    }

    // Route Preference (inline cycle: Fastest → Shortest)
    {
        bool isShortest = settings->routePreference() == QLatin1String("shortest");
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("route_preference"),
            tr->menuRoutePreference(), {
                {tr->menuRouteFastest(),  {Verb::SetSetting, QStringLiteral("route-preference"), QStringLiteral("fastest")}},
                {tr->menuRouteShortest(), {Verb::SetSetting, QStringLiteral("route-preference"), QStringLiteral("shortest")}},
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
                {tr->optOff(),    {Verb::SetSetting, QStringLiteral("avoid-cobblestone"), QStringLiteral("off")}},
                {tr->optLow(),    {Verb::SetSetting, QStringLiteral("avoid-cobblestone"), QStringLiteral("low")}},
                {tr->optMedium(), {Verb::SetSetting, QStringLiteral("avoid-cobblestone"), QStringLiteral("medium")}},
                {tr->optHigh(),   {Verb::SetSetting, QStringLiteral("avoid-cobblestone"), QStringLiteral("high")}},
            }, cobbleIdx))
            ->setIsVisible([ctx]() {
                // Shortest costs by raw distance and never reaches the surface
                // weight. The public Valhalla runs stock tiles and stock
                // costing, so it cannot see sett either: hide rather than offer
                // a control that silently does nothing.
                return ctx.settings->routePreference() != QLatin1String("shortest")
                    && ctx.settings->valhallaUrl() != QLatin1String(AppConfig::valhallaOnlineEndpoint);
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
                {tr->optOff(),      {Verb::SetSetting, QStringLiteral("map-updates"), QStringLiteral("off")}},
                {tr->optNotify(),   {Verb::SetSetting, QStringLiteral("map-updates"), QStringLiteral("notify")}},
                {tr->optDownload(), {Verb::SetSetting, QStringLiteral("map-updates"), QStringLiteral("download")}},
            }, updatesIdx));
    }

    // Check for Updates Now. The automatic check runs at most weekly and only
    // once the modem reports connected, so this is the way to ask on demand,
    // and the only way at all while automatic updates are off.
    if (ctx.mapDownload) {
        auto *checkNode = MenuNode::action(QStringLiteral("map_check_now"),
            tr->menuMapCheckNow(), {Verb::CheckMapUpdates},
            [ctx]() { return ctx.mapDownload && ctx.mapDownload->hasMapsInstalled(); });
        checkNode->setValueLabel(lastCheckLabel(tr, ctx.mapDownload->lastUpdateCheck()));
        mapNavNode->addChild(checkNode);
    }

    // Map Type (inline cycle: Online → Offline)
    {
        int mapType = settings->mapType();
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("map_type"),
            tr->menuMapType(), {
                {tr->menuOnline(),  {Verb::SetSetting, QStringLiteral("map-type"), QStringLiteral("online")}},
                {tr->menuOffline(), {Verb::SetSetting, QStringLiteral("map-type"), QStringLiteral("offline")}},
            }, mapType == 1 ? 1 : 0));
    }

    // Navigation Routing (inline cycle: Online → Offline)
    {
        QString vUrl = settings->valhallaUrl();
        bool isOnlineRouting = (vUrl == QLatin1String(AppConfig::valhallaOnlineEndpoint));
        mapNavNode->addChild(MenuNode::cycleSetting(QStringLiteral("navigation_routing"),
            tr->menuNavRouting(), {
                {tr->menuOnline(),  {Verb::SetSetting, QStringLiteral("valhalla-url"), QLatin1String(AppConfig::valhallaOnlineEndpoint)}},
                {tr->menuOffline(), {Verb::SetSetting, QStringLiteral("valhalla-url"), QLatin1String(AppConfig::valhallaOnDeviceEndpoint)}},
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
                {tr->menuBlinkerIcon(),    {Verb::SetSetting, QStringLiteral("blinker-style"), QStringLiteral("icon")}},
                {tr->menuBlinkerOverlay(), {Verb::SetSetting, QStringLiteral("blinker-style"), QStringLiteral("overlay")}},
            }, blinkerIdx));

        // DBC Blinker LED (toggle) — physical LED on the DBC board.
        bool dbcLedOn = settings->dbcBlinkerLed();
        blinkerNode->addChild(MenuNode::setting(QStringLiteral("settings_dbc_blinker_led"),
            tr->menuDbcBlinkerLed(), dbcLedOn ? 1 : 0,
            {Verb::ToggleSetting, QStringLiteral("dbc-blinker-led")}));
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
            {Verb::ToggleSetting, QStringLiteral("milestone-celebrations")}));
    }

    // Alarm
    auto *alarmNode = MenuNode::submenu(QStringLiteral("settings_alarm"), tr->menuAlarm());
    vehicleNode->addChild(alarmNode);
    bool alarmOn = settings->alarmEnabled();
    bool alarmHonkOn = settings->alarmHonk();
    QString alarmDur = settings->alarmDuration();

    alarmNode->addChild(MenuNode::setting(QStringLiteral("alarm_enabled"), tr->menuAlarmEnable(),
        alarmOn ? 1 : 0, {Verb::ToggleSetting, QStringLiteral("alarm-enabled")}));
    alarmNode->addChild(MenuNode::setting(QStringLiteral("alarm_honk"), tr->menuAlarmHonk(),
        alarmHonkOn ? 1 : 0, {Verb::ToggleSetting, QStringLiteral("alarm-honk")}));

    // Alarm Duration (inline cycle: 10s → 20s → 30s)
    {
        int durIdx = 0;
        if (alarmDur == QLatin1String("20")) durIdx = 1;
        else if (alarmDur == QLatin1String("30")) durIdx = 2;
        alarmNode->addChild(MenuNode::cycleSetting(QStringLiteral("alarm_duration"),
            tr->menuAlarmDuration(), {
                {tr->menuAlarmDuration10(), {Verb::SetSetting, QStringLiteral("alarm-duration"), 10}},
                {tr->menuAlarmDuration20(), {Verb::SetSetting, QStringLiteral("alarm-duration"), 20}},
                {tr->menuAlarmDuration30(), {Verb::SetSetting, QStringLiteral("alarm-duration"), 30}},
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
                {QStringLiteral("English"), {Verb::SetSetting, QStringLiteral("language"), QStringLiteral("en")}},
                {QStringLiteral("Deutsch"), {Verb::SetSetting, QStringLiteral("language"), QStringLiteral("de")}},
            }, langIdx));
    }

    // Battery Mode (inline cycle: Single → Dual) — set-once on install.
    {
        bool dualBatt = settings->dualBattery();
        vehicleNode->addChild(MenuNode::cycleSetting(QStringLiteral("settings_battery_mode"),
            tr->menuBatteryMode(), {
                {tr->menuBatterySingle(), {Verb::SetSetting, QStringLiteral("dual-battery"), false}},
                {tr->menuBatteryDual(),   {Verb::SetSetting, QStringLiteral("dual-battery"), true}},
            }, dualBatt ? 1 : 0));
    }

    // Horn While Seatbox Open (toggle) — off mutes the manual horn when the
    // open seat lid rests on the button. Default off; on restores legacy honk.
    {
        bool hornSeatboxOpen = settings->hornWhenSeatboxOpen();
        vehicleNode->addChild(MenuNode::setting(QStringLiteral("settings_horn_seatbox_open"),
            tr->menuHornSeatboxOpen(), hornSeatboxOpen ? 1 : 0,
            {Verb::ToggleSetting, QStringLiteral("horn-when-seatbox-open")}));
    }

    systemNode->addChild(MenuNode::action(QStringLiteral("enter_ums"), tr->menuEnterUms(),
        {Verb::ShowScreen, QStringLiteral("update-mode-info")}));

    systemNode->addChild(MenuNode::action(QStringLiteral("enable_service_mode"),
        tr->menuServiceMode(), {Verb::EnableServiceMode},
        [ctx]() {
            return !(ctx.settings && ctx.settings->serviceActive() == QLatin1String("true"));
        }));

    // Clearing paired phones is the only way to reclaim a scooter's bond list:
    // the firmware accepts a single-bond delete and does nothing with it, so
    // this is all or nothing. Marked caution because it unpairs every phone
    // including the one its owner is holding.
    //
    // Parked only. Re-pairing needs this dashboard to show the passkey, so
    // offering it in a state where the rider cannot immediately pair again
    // would strand them.
    auto *clearBondsNode = systemNode->addChild(MenuNode::action(
        QStringLiteral("clear_paired_phones"),
        tr->menuClearPairedPhones(), {Verb::ClearBluetoothBonds},
        [ctx]() {
            return ctx.vehicle &&
                   ctx.vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Parked);
        }));
    clearBondsNode->setCaution(true);

    // Updates — ordered by how often a rider has any business touching them:
    // how often to look, look now, and then the two that the defaults already
    // get right. All four write the MDB and DBC keys together (see
    // SettingsService::writeOtaSetting): the two boards ship as a pair.
    if (ctx.updateChannel) {
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
                options.append({intervals[i].label,
                                {Verb::SetSetting, QStringLiteral("ota-check-interval"), value}});
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
                tr->menuUpdateCheckNow(), {Verb::CheckOsUpdates});
            checkNode->setValueLabel(lastCheckLabel(tr, settings->otaLastCheck()));
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
                const bool isCurrent = (c.value == current);
                typeNode->addChild(MenuNode::setting(QLatin1String(c.id), c.label,
                    isCurrent ? 1 : 0, {Verb::SetOtaMethod, {}, c.value}));
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
                                                 : ctx.updateChannel->currentChannel();
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
                const bool isCurrent = (c.value == current);
                channelNode->addChild(MenuNode::setting(QLatin1String(c.id), c.label,
                    isCurrent ? 1 : 0, {Verb::SwitchUpdateChannel, {}, c.value}));
            }
        }
    }

    // Capture Logs — `ssh mdb lsc logs`, run by the interpreter in the
    // background with toast feedback.
    systemNode->addChild(MenuNode::action(QStringLiteral("capture_logs"),
                                          tr->menuCaptureLogs(), {Verb::CaptureLogs}));

    // === Info (root level) ===
    //
    // Read-only diagnostics, not settings, and the thing support and
    // connectivity onboarding point people at. Under Settings > System it took
    // five screens to reach an IMEI; from the root it takes three. About moves
    // in with it, being the same kind of read-only page, which keeps the root
    // menu the same length as before.
    {
        const int activeFaults = ctx.faults ? ctx.faults->activeCount() : 0;
        auto withCount = [activeFaults](const QString &label) {
            return activeFaults > 0 ? QStringLiteral("%1 (%2)").arg(label).arg(activeFaults)
                                    : label;
        };

        auto *infoNode = MenuNode::submenu(QStringLiteral("info"), withCount(tr->menuInfo()),
                                           tr->menuInfo().toUpper());
        root->addChild(infoNode);

        struct Page { const char *id; QString title; int page; };
        const Page pages[] = {
            {"info_components", tr->menuInfoComponents(), Navigator::SystemInfoDevice},
            {"info_connectivity", tr->menuInfoConnectivity(), Navigator::SystemInfoConnectivity},
            {"info_batteries", tr->menuInfoBatteries(), Navigator::SystemInfoBatteries},
            {"info_maps", tr->menuInfoMaps(), Navigator::SystemInfoMaps},
        };
        for (const auto &p : pages) {
            infoNode->addChild(MenuNode::action(QLatin1String(p.id), p.title,
                {Verb::ShowSystemInfo, {}, p.page}));
        }

        infoNode->addChild(MenuNode::action(QStringLiteral("faults"),
                                            withCount(tr->menuFaults()),
                                            {Verb::ShowScreen, QStringLiteral("faults")}));

        infoNode->addChild(MenuNode::action(QStringLiteral("about"), tr->menuAbout(),
                                            {Verb::ShowScreen, QStringLiteral("about")}));
    }

    root->addChild(MenuNode::action(QStringLiteral("exit"), tr->menuExit(),
                                    {Verb::CloseMenu}));

    return root;
}

}
