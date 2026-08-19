#include "SettingsStore.h"

SettingsStore::SettingsStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings SettingsStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("settings"), 5000,
        {
            {QStringLiteral("theme"), QStringLiteral("dashboard.theme")},
            {QStringLiteral("mode"), QStringLiteral("dashboard.mode")},
            {QStringLiteral("backlightMode"), QStringLiteral("dashboard.backlight-mode")},
            {QStringLiteral("showRawSpeed"), QStringLiteral("dashboard.show-raw-speed")},
            {QStringLiteral("batteryDisplayMode"), QStringLiteral("dashboard.battery-display-mode")},
            {QStringLiteral("mapType"), QStringLiteral("dashboard.map.type")},
            {QStringLiteral("mapViewMode"), QStringLiteral("dashboard.map.view-mode")},
            {QStringLiteral("mapNorthOriented"), QStringLiteral("dashboard.map.north-oriented")},
            {QStringLiteral("mapRenderMode"), QStringLiteral("dashboard.map.render-mode")},
            {QStringLiteral("valhallaUrl"), QStringLiteral("dashboard.valhalla-url")},
            {QStringLiteral("routePreference"), QStringLiteral("dashboard.route-preference")},
            {QStringLiteral("avoidCobblestone"), QStringLiteral("dashboard.avoid-cobblestone")},
            {QStringLiteral("language"), QStringLiteral("dashboard.language")},
            {QStringLiteral("powerDisplayMode"), QStringLiteral("dashboard.power-display-mode")},
            {QStringLiteral("blinkerStyle"), QStringLiteral("dashboard.blinker-style")},
            {QStringLiteral("dbcBlinkerLed"), QStringLiteral("scooter.dbc-blinker-led")},
            {QStringLiteral("dualBattery"), QStringLiteral("scooter.dual-battery")},
            {QStringLiteral("hornWhenSeatboxOpen"), QStringLiteral("scooter.horn-when-seatbox-open")},
            {QStringLiteral("showGps"), QStringLiteral("dashboard.show-gps")},
            {QStringLiteral("showBluetooth"), QStringLiteral("dashboard.show-bluetooth")},
            {QStringLiteral("showCloud"), QStringLiteral("dashboard.show-cloud")},
            {QStringLiteral("showInternet"), QStringLiteral("dashboard.show-internet")},
            {QStringLiteral("showClock"), QStringLiteral("dashboard.show-clock")},
            {QStringLiteral("showTemperature"), QStringLiteral("dashboard.show-temperature")},
            {QStringLiteral("showCbBattery"), QStringLiteral("dashboard.show-cb-battery")},
            {QStringLiteral("showAuxBattery"), QStringLiteral("dashboard.show-aux-battery")},
            {QStringLiteral("alarmEnabled"), QStringLiteral("alarm.enabled")},
            {QStringLiteral("alarmHonk"), QStringLiteral("alarm.honk")},
            {QStringLiteral("alarmDuration"), QStringLiteral("alarm.duration")},
            {QStringLiteral("hopOnCombo"), QStringLiteral("dashboard.hop-on-combo")},
            {QStringLiteral("mapCheckForUpdates"), QStringLiteral("dashboard.maps.check-for-updates")},
            {QStringLiteral("mapAutoDownload"), QStringLiteral("dashboard.maps.auto-download")},
            {QStringLiteral("mapTrafficOverlay"), QStringLiteral("dashboard.map.traffic-overlay")},
            {QStringLiteral("milestoneCelebrations"), QStringLiteral("dashboard.milestone-celebrations")},
            {QStringLiteral("serviceActive"), QStringLiteral("dashboard.service-mode-active")},
            {QStringLiteral("otaChannel"), QStringLiteral("updates.mdb.channel")},
            {QStringLiteral("otaMethod"), QStringLiteral("updates.mdb.method")},
            {QStringLiteral("otaCheckInterval"), QStringLiteral("updates.mdb.check-interval")},
            {QStringLiteral("otaLastCheck"), QStringLiteral("updates.mdb.last-check-time")},
        },
        {}, {}
    };
}

void SettingsStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("dashboard.theme")) {
        if (value != m_theme) { m_theme = value; emit themeChanged(); }
    } else if (variable == QLatin1String("dashboard.mode")) {
        if (value != m_mode) { m_mode = value; emit modeChanged(); }
    } else if (variable == QLatin1String("dashboard.backlight-mode")) {
        if (value != m_backlightMode) { m_backlightMode = value; emit backlightModeChanged(); }
    } else if (variable == QLatin1String("dashboard.show-raw-speed")) {
        if (value != m_showRawSpeed) { m_showRawSpeed = value; emit showRawSpeedChanged(); }
    } else if (variable == QLatin1String("dashboard.battery-display-mode")) {
        if (value != m_batteryDisplayMode) { m_batteryDisplayMode = value; emit batteryDisplayModeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.type")) {
        auto v = ScootEnums::parseMapType(value);
        if (v != m_mapType) { m_mapType = v; emit mapTypeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.view-mode")) {
        auto v = ScootEnums::parseMapViewMode(value);
        if (v != m_mapViewMode) { m_mapViewMode = v; emit mapViewModeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.north-oriented")) {
        if (value != m_mapNorthOriented) { m_mapNorthOriented = value; emit mapNorthOrientedChanged(); }
    } else if (variable == QLatin1String("dashboard.map.render-mode")) {
        auto v = ScootEnums::parseMapRenderMode(value);
        if (v != m_mapRenderMode) { m_mapRenderMode = v; emit mapRenderModeChanged(); }
    } else if (variable == QLatin1String("dashboard.valhalla-url")) {
        if (value != m_valhallaUrl) { m_valhallaUrl = value; emit valhallaUrlChanged(); }
    } else if (variable == QLatin1String("dashboard.route-preference")) {
        if (value != m_routePreference) { m_routePreference = value; emit routePreferenceChanged(); }
    } else if (variable == QLatin1String("dashboard.avoid-cobblestone")) {
        if (value != m_avoidCobblestone) { m_avoidCobblestone = value; emit avoidCobblestoneChanged(); }
    } else if (variable == QLatin1String("dashboard.language")) {
        if (value != m_language) { m_language = value; emit languageChanged(); }
    } else if (variable == QLatin1String("dashboard.power-display-mode")) {
        auto v = ScootEnums::parsePowerDisplayMode(value);
        if (v != m_powerDisplayMode) { m_powerDisplayMode = v; emit powerDisplayModeChanged(); }
    } else if (variable == QLatin1String("dashboard.blinker-style")) {
        if (value != m_blinkerStyle) { m_blinkerStyle = value; emit blinkerStyleChanged(); }
    } else if (variable == QLatin1String("scooter.dbc-blinker-led")) {
        if (value != m_dbcBlinkerLed) { m_dbcBlinkerLed = value; emit dbcBlinkerLedChanged(); }
    } else if (variable == QLatin1String("scooter.dual-battery")) {
        if (value != m_dualBattery) { m_dualBattery = value; emit dualBatteryChanged(); }
    } else if (variable == QLatin1String("scooter.horn-when-seatbox-open")) {
        if (value != m_hornWhenSeatboxOpen) { m_hornWhenSeatboxOpen = value; emit hornWhenSeatboxOpenChanged(); }
    } else if (variable == QLatin1String("dashboard.show-gps")) {
        if (value != m_showGps) { m_showGps = value; emit showGpsChanged(); }
    } else if (variable == QLatin1String("dashboard.show-bluetooth")) {
        if (value != m_showBluetooth) { m_showBluetooth = value; emit showBluetoothChanged(); }
    } else if (variable == QLatin1String("dashboard.show-cloud")) {
        if (value != m_showCloud) { m_showCloud = value; emit showCloudChanged(); }
    } else if (variable == QLatin1String("dashboard.show-internet")) {
        if (value != m_showInternet) { m_showInternet = value; emit showInternetChanged(); }
    } else if (variable == QLatin1String("dashboard.show-clock")) {
        if (value != m_showClock) { m_showClock = value; emit showClockChanged(); }
    } else if (variable == QLatin1String("dashboard.show-temperature")) {
        if (value != m_showTemperature) { m_showTemperature = value; emit showTemperatureChanged(); }
    } else if (variable == QLatin1String("dashboard.show-cb-battery")) {
        if (value != m_showCbBattery) { m_showCbBattery = value; emit showCbBatteryChanged(); }
    } else if (variable == QLatin1String("dashboard.show-aux-battery")) {
        if (value != m_showAuxBattery) { m_showAuxBattery = value; emit showAuxBatteryChanged(); }
    } else if (variable == QLatin1String("alarm.enabled")) {
        if (value != m_alarmEnabled) { m_alarmEnabled = value; emit alarmEnabledChanged(); }
    } else if (variable == QLatin1String("alarm.honk")) {
        if (value != m_alarmHonk) { m_alarmHonk = value; emit alarmHonkChanged(); }
    } else if (variable == QLatin1String("alarm.duration")) {
        if (value != m_alarmDuration) { m_alarmDuration = value; emit alarmDurationChanged(); }
    } else if (variable == QLatin1String("dashboard.hop-on-combo")) {
        if (value != m_hopOnCombo) { m_hopOnCombo = value; emit hopOnComboChanged(); }
    } else if (variable == QLatin1String("dashboard.maps.check-for-updates")) {
        if (value != m_mapCheckForUpdates) { m_mapCheckForUpdates = value; emit mapCheckForUpdatesChanged(); }
    } else if (variable == QLatin1String("dashboard.maps.auto-download")) {
        if (value != m_mapAutoDownload) { m_mapAutoDownload = value; emit mapAutoDownloadChanged(); }
    } else if (variable == QLatin1String("dashboard.map.traffic-overlay")) {
        if (value != m_mapTrafficOverlay) { m_mapTrafficOverlay = value; emit mapTrafficOverlayChanged(); }
    } else if (variable == QLatin1String("dashboard.milestone-celebrations")) {
        if (value != m_milestoneCelebrations) { m_milestoneCelebrations = value; emit milestoneCelebrationsChanged(); }
    } else if (variable == QLatin1String("dashboard.service-mode-active")) {
        if (value != m_serviceActive) {
            m_serviceActive = value;
            emit serviceActiveChanged();
        }
    } else if (variable == QLatin1String("updates.mdb.channel")) {
        if (value != m_otaChannel) { m_otaChannel = value; emit otaChannelChanged(); }
    } else if (variable == QLatin1String("updates.mdb.method")) {
        if (value != m_otaMethod) { m_otaMethod = value; emit otaMethodChanged(); }
    } else if (variable == QLatin1String("updates.mdb.check-interval")) {
        if (value != m_otaCheckInterval) { m_otaCheckInterval = value; emit otaCheckIntervalChanged(); }
    } else if (variable == QLatin1String("updates.mdb.last-check-time")) {
        if (value != m_otaLastCheck) { m_otaLastCheck = value; emit otaLastCheckChanged(); }
    }
}
