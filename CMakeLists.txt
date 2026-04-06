cmake_minimum_required(VERSION 3.16)
project(scootui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(DESKTOP_MODE "Build for desktop development with simulator panel (QMapLibre optional)" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Qml Svg Network Sql Concurrent)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)

if(DESKTOP_MODE)
    find_package(QMapLibre QUIET COMPONENTS Core)
    if(NOT QMapLibre_FOUND)
        find_package(QMapLibre QUIET)  # Try legacy (no components)
    endif()
    find_package(Qt6 COMPONENTS QuickControls2 REQUIRED)
else()
    find_package(QMapLibre QUIET COMPONENTS Core)
    if(NOT QMapLibre_FOUND)
        find_package(QMapLibre REQUIRED)  # Legacy fallback
    endif()
endif()

# Support both old (QMapLibre::QMapLibre) and new (QMapLibre::Core) target names
if(TARGET QMapLibre::Core)
    set(QMAPLIBRE_TARGET QMapLibre::Core)
elseif(TARGET QMapLibre::QMapLibre)
    set(QMAPLIBRE_TARGET QMapLibre::QMapLibre)
endif()

if(QMapLibre_FOUND AND QMAPLIBRE_TARGET)
    find_package(Qt6 COMPONENTS OpenGL REQUIRED)
    set(HAS_QMAPLIBRE TRUE)
    add_definitions(-DHAS_QMAPLIBRE)
    message(STATUS "QMapLibre found - map rendering enabled (target: ${QMAPLIBRE_TARGET})")
else()
    set(HAS_QMAPLIBRE FALSE)
    message(STATUS "QMapLibre not found - map rendering disabled (placeholder will be shown)")
endif()

if(DESKTOP_MODE)
    add_definitions(-DDESKTOP_MODE)
    message(STATUS "Desktop mode enabled - simulator panel available")
endif()

qt_standard_project_setup(REQUIRES 6.4)

set(CPP_SOURCES
    src/main.cpp
    src/core/EnvConfig.cpp
    src/core/Application.cpp
    src/repositories/InMemoryMdbRepository.cpp
    src/repositories/RedisMdbRepository.cpp
    src/repositories/HiredisWorker.cpp
    src/stores/SyncableStore.cpp
    src/stores/EngineStore.cpp
    src/stores/VehicleStore.cpp
    src/stores/BatteryStore.cpp
    src/stores/GpsStore.cpp
    src/stores/BluetoothStore.cpp
    src/stores/InternetStore.cpp
    src/stores/NavigationStore.cpp
    src/stores/SettingsStore.cpp
    src/stores/OtaStore.cpp
    src/stores/UsbStore.cpp
    src/stores/UmsLogStore.cpp
    src/stores/SpeedLimitStore.cpp
    src/stores/AutoStandbyStore.cpp
    src/stores/CbBatteryStore.cpp
    src/stores/AuxBatteryStore.cpp
    src/stores/ThemeStore.cpp
    src/stores/ScreenStore.cpp
    src/stores/MenuStore.cpp
    src/stores/TripStore.cpp
    src/stores/ShutdownStore.cpp
    src/stores/LocaleStore.cpp
    src/stores/ShortcutMenuStore.cpp
    src/stores/ConnectionStore.cpp
    src/stores/DashboardStore.cpp
    src/stores/SavedLocationsStore.cpp
    src/services/SettingsService.cpp
    src/services/AutoThemeService.cpp
    src/services/InputHandler.cpp
    src/services/NavigationService.cpp
    src/services/ToastService.cpp
    src/services/MapService.cpp
    src/services/LowTemperatureMonitor.cpp
    src/services/BluetoothHealthMonitor.cpp
    src/services/HandlebarLockMonitor.cpp
    src/services/NavigationAvailabilityService.cpp
    src/services/SavedLocationsService.cpp
    src/services/ReverseGeocodingService.cpp
    src/services/SerialNumberService.cpp
    src/services/AddressDatabaseService.cpp
    src/services/SystemInfoService.cpp
    src/services/VectorTileDecoder.cpp
    src/services/MapDownloadService.cpp
    src/services/RoadInfoService.cpp
    src/routing/ValhallaClient.cpp
    src/l10n/Translations.cpp
    src/utils/FaultFormatter.cpp
)

set(CPP_HEADERS
    src/core/AppConfig.h
    src/core/EnvConfig.h
    src/core/Application.h
    src/models/Enums.h
    src/models/MenuNode.h
    src/models/SavedLocation.h
    src/repositories/MdbRepository.h
    src/repositories/InMemoryMdbRepository.h
    src/repositories/RedisMdbRepository.h
    src/repositories/HiredisAdapter.h
    src/repositories/HiredisWorker.h
    src/stores/SyncableStore.h
    src/stores/EngineStore.h
    src/stores/VehicleStore.h
    src/stores/BatteryStore.h
    src/stores/GpsStore.h
    src/stores/BluetoothStore.h
    src/stores/InternetStore.h
    src/stores/NavigationStore.h
    src/stores/SettingsStore.h
    src/stores/OtaStore.h
    src/stores/UsbStore.h
    src/stores/UmsLogStore.h
    src/stores/SpeedLimitStore.h
    src/stores/AutoStandbyStore.h
    src/stores/CbBatteryStore.h
    src/stores/AuxBatteryStore.h
    src/stores/ThemeStore.h
    src/stores/ScreenStore.h
    src/stores/MenuStore.h
    src/stores/TripStore.h
    src/stores/ShutdownStore.h
    src/stores/LocaleStore.h
    src/stores/ShortcutMenuStore.h
    src/stores/ConnectionStore.h
    src/stores/DashboardStore.h
    src/stores/SavedLocationsStore.h
    src/services/SettingsService.h
    src/services/AutoThemeService.h
    src/services/InputHandler.h
    src/services/NavigationService.h
    src/services/ToastService.h
    src/services/MapService.h
    src/services/LowTemperatureMonitor.h
    src/services/BluetoothHealthMonitor.h
    src/services/HandlebarLockMonitor.h
    src/services/NavigationAvailabilityService.h
    src/services/SavedLocationsService.h
    src/services/ReverseGeocodingService.h
    src/services/SerialNumberService.h
    src/services/AddressDatabaseService.h
    src/services/SystemInfoService.h
    src/services/VectorTileDecoder.h
    src/services/MapDownloadService.h
    src/services/RoadInfoService.h
    src/models/MapMetadata.h
    src/routing/RouteModels.h
    src/routing/RouteHelpers.h
    src/routing/ValhallaClient.h
    src/l10n/Translations.h
    src/utils/FaultFormatter.h
)

# Simulator sources (always compiled, only activated at runtime in sim mode)
list(APPEND CPP_SOURCES src/simulator/SimulatorService.cpp)
list(APPEND CPP_HEADERS src/simulator/SimulatorService.h)

qt_add_executable(scootui ${CPP_SOURCES} ${CPP_HEADERS})

set_target_properties(scootui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set(QML_FILES
    qml/Main.qml
    qml/screens/ClusterScreen.qml
    qml/screens/MaintenanceScreen.qml
    qml/screens/AboutScreen.qml
    qml/screens/DebugScreen.qml
    qml/screens/OtaBackgroundScreen.qml
    qml/screens/MapScreen.qml
    qml/screens/AddressSelectionScreen.qml
    qml/screens/NavigationSetupScreen.qml
    qml/screens/DestinationScreen.qml
    qml/widgets/navigation/TurnByTurnWidget.qml
    qml/widgets/navigation/NavigationStatusOverlay.qml
    qml/widgets/navigation/RoundaboutIcon.qml
    qml/widgets/speedometer/SpeedometerDisplay.qml
    qml/widgets/status_bars/TopStatusBar.qml
    qml/widgets/status_bars/BatteryDisplay.qml
    qml/widgets/status_bars/BatteryIcon.qml
    qml/widgets/status_bars/UnifiedBottomStatusBar.qml
    qml/widgets/status_bars/SpeedCenterWidget.qml
    qml/widgets/cluster/BlinkerRow.qml
    qml/widgets/cluster/ClusterBottomBar.qml
    qml/widgets/power/PowerDisplay.qml
    qml/widgets/indicators/IndicatorLight.qml
    qml/widgets/indicators/IndicatorLights.qml
    qml/widgets/indicators/StatusIndicators.qml
    qml/widgets/indicators/SpeedLimitIndicator.qml
    qml/widgets/indicators/RoadNameDisplay.qml
    qml/widgets/blinker/BlinkerOverlay.qml
    qml/widgets/shutdown/ShutdownOverlay.qml
    qml/widgets/components/SvgIcon.qml
    qml/widgets/components/ControlHints.qml
    qml/widgets/components/MaterialIcon.qml
    qml/widgets/components/TintedImage.qml
    qml/widgets/components/FrostedGlass.qml
    qml/widgets/map/MapViewWidget.qml
    qml/widgets/map/MapViewContent.qml
    qml/widgets/map/MapViewContentLegacy.qml
    qml/widgets/map/VehicleMarker.qml
    qml/widgets/map/NorthIndicator.qml
    qml/widgets/map/ScaleBar.qml
    qml/overlays/MenuOverlay.qml
    qml/overlays/MenuItem.qml
    qml/overlays/ShortcutMenuOverlay.qml
    qml/overlays/UmsOverlay.qml
    qml/overlays/BluetoothPinCodeOverlay.qml
    qml/overlays/ManualHibernationOverlay.qml
    qml/overlays/VersionOverlay.qml
    qml/overlays/ToastOverlay.qml
    qml/overlays/DebugOverlay.qml
    qml/simulator/SimulatorWindow.qml
)

qt_add_qml_module(scootui
    URI ScootUI
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        assets/icons/librescoot-turn-left.svg
        assets/icons/librescoot-turn-right.svg
        assets/icons/librescoot-hazards.svg
        assets/icons/librescoot-engine-warning.svg
        assets/icons/librescoot-parking-brake.svg
        assets/icons/librescoot-seatbox-open.svg
        assets/icons/librescoot-turtle-mode.svg
        assets/icons/librescoot-propulsion-battery-fault.svg
        assets/icons/librescoot-main-battery-blank.svg
        assets/icons/librescoot-main-battery-empty.svg
        assets/icons/librescoot-main-battery-absent.svg
        assets/icons/librescoot-main-battery-asleep.svg
        assets/icons/librescoot-main-battery-asleep-mask.svg
        assets/icons/librescoot-main-battery-asleep-overlay.svg
        assets/icons/librescoot-main-battery-fault.svg
        assets/icons/librescoot-main-battery-locked.svg
        assets/icons/librescoot-cb-battery-blank.svg
        assets/icons/librescoot-cb-battery-level-0.svg
        assets/icons/librescoot-cb-battery-level-25.svg
        assets/icons/librescoot-cb-battery-level-50.svg
        assets/icons/librescoot-cb-battery-level-75.svg
        assets/icons/librescoot-cb-battery-level-100.svg
        assets/icons/librescoot-aux-battery-blank.svg
        assets/icons/librescoot-aux-battery-level-0.svg
        assets/icons/librescoot-aux-battery-level-25.svg
        assets/icons/librescoot-aux-battery-level-50.svg
        assets/icons/librescoot-aux-battery-level-75.svg
        assets/icons/librescoot-aux-battery-level-100.svg
        assets/icons/librescoot-overlay-error.svg
        assets/icons/librescoot-overlay-error-light.svg
        assets/icons/librescoot-overlay-locked.svg
        assets/icons/librescoot-overlay-slashed.svg
        assets/icons/librescoot-overlay-charging.svg
        assets/icons/librescoot-overlay-asleep.svg
        assets/icons/librescoot-overlay-idle.svg
        assets/icons/librescoot-overlay-idle-light.svg
        assets/icons/librescoot-bluetooth-connected.svg
        assets/icons/librescoot-bluetooth-disconnected.svg
        assets/icons/librescoot-bluetooth-error.svg
        assets/icons/librescoot-gps-off.svg
        assets/icons/librescoot-gps-searching.svg
        assets/icons/librescoot-gps-center-dot.svg
        assets/icons/librescoot-gps-fix-established.svg
        assets/icons/librescoot-gps-error.svg
        assets/icons/librescoot-internet-cloud-connected.svg
        assets/icons/librescoot-internet-cloud-disconnected.svg
        assets/icons/librescoot-internet-modem-off.svg
        assets/icons/librescoot-internet-modem-disconnected.svg
        assets/icons/librescoot-internet-modem-connected-0.svg
        assets/icons/librescoot-internet-modem-connected-1.svg
        assets/icons/librescoot-internet-modem-connected-2.svg
        assets/icons/librescoot-internet-modem-connected-3.svg
        assets/icons/librescoot-internet-modem-connected-4.svg
        assets/icons/librescoot-ota-status-downloading.svg
        assets/icons/librescoot-ota-status-installing.svg
        assets/icons/librescoot-ota-status-waiting-for-reboot.svg
        assets/icons/speedlimit_none.svg
        assets/icons/speedlimit_unknown.svg
        assets/icons/speedlimit_blank.svg
        assets/icons/librescoot-logo.png
        assets/icons/librescoot-logotype.svg
        assets/icons/nav-setup-qr.png
        assets/routes/route1.json
        assets/routes/route2.json
        assets/fonts/Roboto-Regular.ttf
        assets/fonts/Roboto-Bold.ttf
        assets/fonts/Roboto-Medium.ttf
        assets/fonts/Roboto-Light.ttf
        assets/fonts/RobotoCondensed-Bold.ttf
        assets/fonts/RobotoCondensed-Regular.ttf
        assets/fonts/MaterialIcons-Regular.otf
        assets/styles/mapdark.json
        assets/styles/maplight.json
)

target_include_directories(scootui PRIVATE ${HIREDIS_INCLUDE_DIRS})

target_link_libraries(scootui PRIVATE
    Qt6::Quick
    Qt6::Svg
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    ZLIB::ZLIB
    ${HIREDIS_LIBRARIES}
)

if(HAS_QMAPLIBRE)
    target_link_libraries(scootui PRIVATE Qt6::OpenGL ${QMAPLIBRE_TARGET})
endif()

if(DESKTOP_MODE)
    target_link_libraries(scootui PRIVATE Qt6::QuickControls2)
endif()

target_include_directories(scootui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS scootui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
