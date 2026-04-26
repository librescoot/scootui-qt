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
            {QStringLiteral("showRawSpeed"), QStringLiteral("dashboard.show-raw-speed")},
            {QStringLiteral("batteryDisplayMode"), QStringLiteral("dashboard.battery-display-mode")},
            {QStringLiteral("mapType"), QStringLiteral("dashboard.map.type")},
            {QStringLiteral("mapRenderMode"), QStringLiteral("dashboard.map.render-mode")},
            {QStringLiteral("valhallaUrl"), QStringLiteral("dashboard.valhalla-url")},
            {QStringLiteral("language"), QStringLiteral("dashboard.language")},
            {QStringLiteral("powerDisplayMode"), QStringLiteral("dashboard.power-display-mode")},
            {QStringLiteral("blinkerStyle"), QStringLiteral("dashboard.blinker-style")},
            {QStringLiteral("dbcBlinkerLed"), QStringLiteral("scooter.dbc-blinker-led")},
            {QStringLiteral("dualBattery"), QStringLiteral("scooter.dual-battery")},
            {QStringLiteral("showGps"), QStringLiteral("dashboard.show-gps")},
            {QStringLiteral("showBluetooth"), QStringLiteral("dashboard.show-bluetooth")},
            {QStringLiteral("showCloud"), QStringLiteral("dashboard.show-cloud")},
            {QStringLiteral("showInternet"), QStringLiteral("dashboard.show-internet")},
            {QStringLiteral("showClock"), QStringLiteral("dashboard.show-clock")},
            {QStringLiteral("showTemperature"), QStringLiteral("dashboard.show-temperature")},
            {QStringLiteral("alarmEnabled"), QStringLiteral("alarm.enabled")},
            {QStringLiteral("alarmHonk"), QStringLiteral("alarm.honk")},
            {QStringLiteral("alarmDuration"), QStringLiteral("alarm.duration")},
            {QStringLiteral("hopOnCombo"), QStringLiteral("dashboard.hop-on-combo")},
            {QStringLiteral("mapCheckForUpdates"), QStringLiteral("dashboard.maps.check-for-updates")},
            {QStringLiteral("mapAutoDownload"), QStringLiteral("dashboard.maps.auto-download")},
            {QStringLiteral("mapTrafficOverlay"), QStringLiteral("dashboard.map.traffic-overlay")},
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
    } else if (variable == QLatin1String("dashboard.show-raw-speed")) {
        if (value != m_showRawSpeed) { m_showRawSpeed = value; emit showRawSpeedChanged(); }
    } else if (variable == QLatin1String("dashboard.battery-display-mode")) {
        if (value != m_batteryDisplayMode) { m_batteryDisplayMode = value; emit batteryDisplayModeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.type")) {
        auto v = ScootEnums::parseMapType(value);
        if (v != m_mapType) { m_mapType = v; emit mapTypeChanged(); }
    } else if (variable == QLatin1String("dashboard.map.render-mode")) {
        auto v = ScootEnums::parseMapRenderMode(value);
        if (v != m_mapRenderMode) { m_mapRenderMode = v; emit mapRenderModeChanged(); }
    } else if (variable == QLatin1String("dashboard.valhalla-url")) {
        if (value != m_valhallaUrl) { m_valhallaUrl = value; emit valhallaUrlChanged(); }
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
    }
}
