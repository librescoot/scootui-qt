cmake_minimum_required(VERSION 3.16)
project(scootui VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_TESTING "Build the ScootUI unit and scenario tests" OFF)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(DESKTOP_MODE "Build for desktop development with simulator panel (QMapLibre optional)" OFF)

# Escape hatch for build environments that have no QMapLibre (the armhf
# cross-build container, see cross-build.sh). Produces a binary with no map
# rendering. Never set this for a device or Yocto build.
option(SCOOTUI_ALLOW_MISSING_QMAPLIBRE "Configure without QMapLibre instead of failing (reduced build: no map rendering)" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick Qml Svg Network Sql Concurrent)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(ZSTD REQUIRED libzstd)

if(DESKTOP_MODE)
    find_package(QMapLibre QUIET COMPONENTS Core)
    if(NOT QMapLibre_FOUND)
        find_package(QMapLibre QUIET)  # Try legacy (no components)
    endif()
    find_package(Qt6 COMPONENTS QuickControls2 REQUIRED)
else()
    find_package(QMapLibre QUIET COMPONENTS Core)
    if(NOT QMapLibre_FOUND)
        if(SCOOTUI_ALLOW_MISSING_QMAPLIBRE)
            find_package(QMapLibre QUIET)  # Legacy fallback
        else()
            find_package(QMapLibre REQUIRED)  # Legacy fallback
        endif()
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
    if(SCOOTUI_ALLOW_MISSING_QMAPLIBRE)
        message(WARNING "SCOOTUI_ALLOW_MISSING_QMAPLIBRE is set and QMapLibre is absent. "
                        "This is a reduced build with no map rendering - not equivalent to a device build.")
    endif()
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
    src/repositories/PubsubWorker.cpp
    src/commands/CommandBus.cpp
    src/stores/SyncableStore.cpp
    src/stores/EngineStore.cpp
    src/stores/VehicleStore.cpp
    src/stores/BatteryStore.cpp
    src/stores/GpsStore.cpp
    src/stores/MotionStore.cpp
    src/stores/BluetoothStore.cpp
    src/stores/InternetStore.cpp
    src/stores/ModemStore.cpp
    src/stores/NavigationStore.cpp
    src/stores/SettingsStore.cpp
    src/stores/OtaStore.cpp
    src/stores/UsbStore.cpp
    src/stores/UmsLogStore.cpp
    src/stores/SpeedLimitStore.cpp
    src/stores/AutoStandbyStore.cpp
    src/stores/ScooterStore.cpp
    src/stores/CbBatteryStore.cpp
    src/stores/AuxBatteryStore.cpp
    src/stores/ThemeStore.cpp
    src/core/Navigator.cpp
    src/controllers/AddressEntryController.cpp
    src/controllers/MapSetupController.cpp
    src/coordinators/MapUpdateCoordinator.cpp
    src/coordinators/ReadinessCoordinator.cpp
    src/menu/MenuController.cpp
    src/menu/MenuActions.cpp
    src/menu/MenuDefinition.cpp
    src/services/HopOnService.cpp
    src/services/TripService.cpp
    src/stores/ShutdownStore.cpp
    src/stores/LocaleStore.cpp
    src/menu/ShortcutMenuController.cpp
    src/stores/ConnectionStore.cpp
    src/stores/DashboardStore.cpp
    src/stores/SavedLocationsStore.cpp
    src/stores/RecentDestinationsStore.cpp
    src/services/SettingsService.cpp
    src/services/AutoThemeService.cpp
    src/services/GestureSynth.cpp
    src/services/InputHandler.cpp
    src/services/NavigationService.cpp
    src/services/ToastService.cpp
    src/services/MapService.cpp
    src/services/LowTemperatureMonitor.cpp
    src/services/BluetoothHealthMonitor.cpp
    src/services/HandlebarLockMonitor.cpp
    src/services/BackupBatteryMonitor.cpp
    src/services/BatteryAlertModel.cpp
    src/services/SystemHealthMonitor.cpp
    src/services/FaultNotifier.cpp
    src/services/NavigationAvailabilityService.cpp
    src/services/SavedLocationsService.cpp
    src/services/RecentDestinationsService.cpp
    src/services/ReverseGeocodingService.cpp
    src/services/SerialNumberService.cpp
    src/services/AddressDatabaseService.cpp
    src/services/FlatTrie.cpp
    src/services/SystemInfoService.cpp
    src/services/VectorTileDecoder.cpp
    src/services/MapDownloadService.cpp
    src/services/UpdateChannelService.cpp
    src/services/RoadInfoService.cpp
    src/services/RoadInfoWorker.cpp
    src/services/OdometerMilestoneService.cpp
    src/routing/ValhallaClient.cpp
    src/l10n/Translations.cpp
    src/utils/FaultFormatter.cpp
    src/utils/ZstdDecompressor.cpp
    src/stores/FaultEventStore.cpp
    src/services/FaultsService.cpp
)

set(CPP_HEADERS
    src/core/AppConfig.h
    src/core/EnvConfig.h
    src/core/Application.h
    src/models/Enums.h
    src/controllers/AddressEntryController.h
    src/controllers/MapSetupController.h
    src/coordinators/MapUpdateCoordinator.h
    src/coordinators/ReadinessCoordinator.h
    src/controllers/MapSetupPolicy.h
    src/menu/MenuNode.h
    src/menu/MenuAction.h
    src/menu/MenuDefinition.h
    src/models/SavedLocation.h
    src/repositories/MdbRepository.h
    src/repositories/InMemoryMdbRepository.h
    src/repositories/RedisMdbRepository.h
    src/repositories/HiredisAdapter.h
    src/repositories/HiredisWorker.h
    src/repositories/PubsubWorker.h
    src/stores/SyncableStore.h
    src/stores/EngineStore.h
    src/stores/VehicleStore.h
    src/stores/BatteryStore.h
    src/stores/GpsStore.h
    src/stores/MotionStore.h
    src/stores/BluetoothStore.h
    src/stores/InternetStore.h
    src/stores/ModemStore.h
    src/stores/NavigationStore.h
    src/stores/SettingsStore.h
    src/stores/OtaStore.h
    src/stores/UsbStore.h
    src/stores/UmsLogStore.h
    src/stores/SpeedLimitStore.h
    src/stores/SpeedLimitParser.h
    src/stores/AutoStandbyStore.h
    src/stores/CbBatteryStore.h
    src/stores/AuxBatteryStore.h
    src/stores/ThemeStore.h
    src/core/Navigator.h
    src/menu/MenuController.h
    src/services/TripService.h
    src/stores/ShutdownStore.h
    src/stores/LocaleStore.h
    src/menu/ShortcutMenuController.h
    src/stores/ConnectionStore.h
    src/stores/DashboardStore.h
    src/stores/SavedLocationsStore.h
    src/services/SettingsService.h
    src/services/AutoThemeService.h
    src/services/GestureSynth.h
    src/services/InputHandler.h
    src/services/NavigationService.h
    src/services/ToastService.h
    src/services/MapService.h
    src/services/MapCameraPolicy.h
    src/services/NavigationCadence.h
    src/services/LowTemperatureMonitor.h
    src/services/BluetoothHealthMonitor.h
    src/services/HandlebarLockMonitor.h
    src/services/BackupBatteryMonitor.h
    src/services/BatteryAlertModel.h
    src/services/SystemHealthMonitor.h
    src/services/FaultNotifier.h
    src/services/BatteryAlertPolicy.h
    src/services/NavigationAvailabilityService.h
    src/services/SavedLocationsService.h
    src/services/ReverseGeocodingService.h
    src/services/SerialNumberService.h
    src/services/AddressDatabaseService.h
    src/services/FlatTrie.h
    src/services/SystemInfoService.h
    src/services/UpdateChannelService.h
    src/services/VectorTileDecoder.h
    src/services/MapDownloadService.h
    src/services/RoadInfoService.h
    src/services/RoadInfoWorker.h
    src/services/RoadMatchPolicy.h
    src/services/OdometerMilestoneService.h
    src/models/MapMetadata.h
    src/routing/RouteModels.h
    src/routing/RouteHelpers.h
    src/routing/ValhallaClient.h
    src/l10n/Translations.h
    src/utils/FaultFormatter.h
    src/utils/ZstdDecompressor.h
    src/stores/FaultEventStore.h
    src/services/FaultsService.h
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
    qml/screens/MotionDebugScreen.qml
    qml/screens/MapScreen.qml
    qml/screens/AddressSelectionScreen.qml
    qml/screens/NavigationSetupScreen.qml
    qml/screens/FaultsScreen.qml
    qml/screens/SystemInfoScreen.qml
    qml/screens/UpdateModeInfoScreen.qml
    qml/screens/UpdateChannelScreen.qml
    qml/screens/HopOnInfoScreen.qml
    qml/widgets/navigation/TurnByTurnWidget.qml
    qml/widgets/navigation/NavigationStatusOverlay.qml
    qml/widgets/navigation/RoundaboutIcon.qml
    qml/widgets/navigation/RoundaboutIconFromMap.qml
    qml/widgets/speedometer/SpeedometerDisplay.qml
    qml/widgets/status_bars/TopStatusBar.qml
    qml/widgets/status_bars/ClockDisplay.qml
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
    qml/widgets/indicators/TelltalePanel.qml
    qml/widgets/indicators/SpeedLimitIndicator.qml
    qml/widgets/indicators/CalendarDayIcon.qml
    qml/widgets/indicators/RoadNameDisplay.qml
    qml/widgets/blinker/BlinkerOverlay.qml
    qml/widgets/shutdown/ShutdownOverlay.qml
    qml/widgets/components/SvgIcon.qml
    qml/widgets/components/OtaStatusIcon.qml
    qml/widgets/components/ControlHints.qml
    qml/widgets/components/MaterialIcon.qml
    qml/widgets/components/TintedImage.qml
    qml/widgets/components/OverflowChip.qml
    qml/widgets/components/FrostedGlass.qml
    qml/widgets/components/MilestoneConfettiLayer.qml
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
    qml/overlays/AutoLockCountdownOverlay.qml
    qml/overlays/HopOnLearnOverlay.qml
    qml/overlays/HopOnLockOverlay.qml
    qml/overlays/VersionOverlay.qml
    qml/overlays/ToastOverlay.qml
    qml/overlays/OdometerMilestoneOverlay.qml
    qml/overlays/MilestoneCelebrationOverlay.qml
    qml/overlays/DebugOverlay.qml
    qml/overlays/ServiceModeOverlay.qml
    qml/simulator/SimulatorWindow.qml
)

set_source_files_properties(qml/widgets/components/MaterialIcon.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(scootui
    URI ScootUI
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        qml/widgets/components/qmldir
        assets/icons/librescoot-turn-left.svg
        assets/icons/librescoot-turn-right.svg
        assets/icons/librescoot-keep-left.svg
        assets/icons/librescoot-keep-left-light.svg
        assets/icons/librescoot-keep-right.svg
        assets/icons/librescoot-keep-right-light.svg
        assets/icons/librescoot-hazards.svg
        assets/icons/librescoot-engine-warning.svg
        assets/icons/librescoot-parking-brake.svg
        assets/icons/librescoot-seatbox-open.svg
        assets/icons/librescoot-brake-left.svg
        assets/icons/librescoot-brake-right.svg
        assets/icons/librescoot-horn.svg
        assets/icons/librescoot-seatbox-button.svg
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
        assets/icons/librescoot-overlay-slashed-light.svg
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
        assets/icons/librescoot-ota-status-preparing.svg
        assets/icons/librescoot-ota-status-installing.svg
        assets/icons/librescoot-ota-status-waiting-for-reboot.svg
        assets/icons/librescoot-ota-status-error.svg
        assets/icons/speedlimit_none.svg
        assets/icons/speedlimit_unknown.svg
        assets/icons/speedlimit_blank.svg
        assets/icons/calendar_blank.svg
        assets/icons/librescoot-logo.png
        assets/icons/librescoot-logotype.svg
        assets/icons/librescoot-logo-small.png
        assets/icons/nav-setup-qr.png
        assets/icons/ums-docs-qr.png
        assets/routes/route1.json
        assets/routes/route2.json
        assets/routes/route3.json
        assets/routes/route4.json
        assets/routes/route5.json
        assets/routes/route6.json
        assets/routes/route7.json
        assets/routes/route1_de.json
        assets/routes/route2_de.json
        assets/routes/route3_de.json
        assets/routes/route4_de.json
        assets/routes/route5_de.json
        assets/routes/route6_de.json
        # Subsetted copies; see scripts/subset-fonts.sh and the originals in
        # assets/fonts/ if you need to regenerate after a font update.
        assets/fonts/subset/Roboto-Regular.ttf
        assets/fonts/subset/Roboto-Bold.ttf
        assets/fonts/subset/Roboto-Medium.ttf
        assets/fonts/subset/Roboto-Light.ttf
        assets/fonts/subset/RobotoCondensed-Bold.ttf
        assets/fonts/subset/RobotoCondensed-Regular.ttf
        assets/fonts/subset/MaterialIcons-Regular.otf
        assets/styles/mapdark.json
        assets/styles/maplight.json
)

target_include_directories(scootui PRIVATE ${HIREDIS_INCLUDE_DIRS})
target_link_directories(scootui PRIVATE ${HIREDIS_LIBRARY_DIRS})

target_link_libraries(scootui PRIVATE
    Qt6::Quick
    Qt6::Svg
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    ZLIB::ZLIB
    ${HIREDIS_LIBRARIES}
    ${ZSTD_LIBRARIES}
)

if(HAS_QMAPLIBRE)
    target_link_libraries(scootui PRIVATE Qt6::OpenGL ${QMAPLIBRE_TARGET})
endif()

if(DESKTOP_MODE)
    target_link_libraries(scootui PRIVATE Qt6::QuickControls2)
endif()

target_include_directories(scootui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(scootui PRIVATE ${ZSTD_INCLUDE_DIRS})
target_link_directories(scootui PRIVATE ${ZSTD_LIBRARY_DIRS})

install(TARGETS scootui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

    qt_add_executable(gps_store_test
        tests/GpsStoreTest.cpp
        src/repositories/MdbRepository.h
        src/stores/GpsStore.cpp
        src/stores/GpsStore.h
        src/stores/SyncableStore.cpp
        src/stores/SyncableStore.h
        src/repositories/InMemoryMdbRepository.cpp
        src/repositories/InMemoryMdbRepository.h
    )
    target_include_directories(gps_store_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gps_store_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME gps_store_test COMMAND gps_store_test)

    qt_add_executable(position_estimator_test
        tests/PositionEstimatorTest.cpp
        src/services/PositionEstimator.h
    )
    target_include_directories(position_estimator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(position_estimator_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME position_estimator_test COMMAND position_estimator_test)

    qt_add_executable(reroute_policy_test
        tests/ReroutePolicyTest.cpp
        src/routing/ReroutePolicy.h
        src/routing/RouteModels.h
    )
    target_include_directories(reroute_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(reroute_policy_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME reroute_policy_test COMMAND reroute_policy_test)

    qt_add_executable(road_match_policy_test
        tests/RoadMatchPolicyTest.cpp
        src/services/RoadMatchPolicy.h
        src/repositories/MdbRepository.h
        src/repositories/InMemoryMdbRepository.cpp
        src/repositories/InMemoryMdbRepository.h
        src/stores/SyncableStore.cpp
        src/stores/SyncableStore.h
        src/stores/SpeedLimitStore.cpp
        src/stores/SpeedLimitStore.h
        src/stores/SpeedLimitParser.h
    )
    target_include_directories(road_match_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(road_match_policy_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME road_match_policy_test COMMAND road_match_policy_test)

    qt_add_executable(map_setup_policy_test
        tests/MapSetupPolicyTest.cpp
        src/controllers/MapSetupPolicy.h
        src/models/Enums.h
    )
    target_include_directories(map_setup_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(map_setup_policy_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME map_setup_policy_test COMMAND map_setup_policy_test)

    qt_add_executable(battery_alert_policy_test
        tests/BatteryAlertPolicyTest.cpp
        src/services/BatteryAlertPolicy.h
        src/models/Enums.h
    )
    target_include_directories(battery_alert_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(battery_alert_policy_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME battery_alert_policy_test COMMAND battery_alert_policy_test)

    qt_add_executable(map_timing_test
        tests/MapTimingTest.cpp
        src/services/MapCameraPolicy.h
        src/services/NavigationCadence.h
        src/routing/RouteModels.h
    )
    target_include_directories(map_timing_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(map_timing_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME map_timing_test COMMAND map_timing_test)
endif()
