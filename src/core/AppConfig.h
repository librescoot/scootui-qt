#pragma once

#include <QString>

struct AppConfig {
    static inline QString settingsFilePath;

    // Redis settings
    static constexpr const char* redisSettingsCluster = "dashboard";
    static constexpr const char* themeSettingKey = "dashboard.theme";
    static constexpr const char* modeSettingKey = "dashboard.mode";
    static constexpr const char* showRawSpeedKey = "dashboard.show-raw-speed";
    static constexpr const char* batteryDisplayModeKey = "dashboard.battery-display-mode";
    static constexpr const char* mapTypeKey = "dashboard.map.type";
    static constexpr const char* mapRenderModeKey = "dashboard.map.render-mode";
    static constexpr const char* redisSettingsPersistentCluster = "settings";

    // Valhalla routing
    static constexpr const char* valhallaOnDeviceEndpoint = "http://127.0.0.1:8002/";
    static constexpr const char* valhallaOnlineEndpoint = "https://valhalla1.openstreetmap.de/";
    static inline QString valhallaEndpoint = QStringLiteral("http://127.0.0.1:8002/");
    static constexpr const char* valhallaEndpointKey = "dashboard.valhalla-url";

    // Brightness / auto-theme
    static constexpr const char* brightnessKey = "brightness";
    static constexpr double autoThemeLightThreshold = 25.0;
    static constexpr double autoThemeDarkThreshold = 15.0;

    // Battery
    static constexpr double maxBatteryRangeKm = 45.0;

    // Settings keys
    static constexpr const char* savedLocationsPrefix = "dashboard.saved-locations";
    static constexpr const char* recentDestinationsPrefix = "dashboard.recent-destinations";
    static constexpr const char* languageSettingKey = "dashboard.language";
    static constexpr const char* mapsAvailableKey = "dashboard.maps-available";
    static constexpr const char* navigationAvailableKey = "dashboard.navigation-available";
    static constexpr const char* blinkerStyleKey = "dashboard.blinker-style";
    static constexpr const char* dualBatteryKey = "scooter.dual-battery";
};
