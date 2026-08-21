#pragma once

#include <QString>

struct AppConfig {
    static inline QString settingsFilePath;

    // Valhalla routing
    static constexpr const char* valhallaOnDeviceEndpoint = "http://127.0.0.1:8002/";
    static constexpr const char* valhallaOnlineEndpoint = "https://valhalla1.openstreetmap.de/";
    static inline QString valhallaEndpoint = QStringLiteral("http://127.0.0.1:8002/");
    static constexpr const char* valhallaEndpointKey = "dashboard.valhalla-url";

    // Auto theme. dbc-backlight publishes the OPT3001 reading in lux to the
    // dashboard hash; these are the only knobs AutoThemeService has.
    static constexpr const char* brightnessKey = "brightness";
    // Hysteresis band. Wide enough that a tree-lined street at night, which
    // swings either side of the lamp level, sits inside it and triggers nothing.
    static constexpr double autoThemeLightThreshold = 20.0;
    static constexpr double autoThemeDarkThreshold = 8.0;
    // How long the reading must stay past the threshold before the theme flips.
    // Long enough to outlast a shadow at riding speed, short enough that a
    // tunnel does not leave a white screen up.
    static constexpr int autoThemeDwellMs = 2500;
    // Minimum time between flips, on top of the dwell.
    static constexpr int autoThemeLockoutMs = 10000;

    // Settings keys
    static constexpr const char* savedLocationsPrefix = "dashboard.saved-locations";
    static constexpr const char* recentDestinationsPrefix = "dashboard.recent-destinations";
};
