#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

class MdbRepository;
class AutoThemeService;
class SettingsService;
class NavigationService;
class Translations;
class InputHandler;
class ShutdownStore;
class QSocketNotifier;
class ToastService;
class MapService;
class LowTemperatureMonitor;
class BluetoothHealthMonitor;
class HandlebarLockMonitor;
class NavigationAvailabilityService;
class SavedLocationsService;
class SerialNumberService;
class AddressDatabaseService;
class SystemInfoService;
class SimulatorService;
class MapDownloadService;
class RoadInfoService;
class EngineStore;
class VehicleStore;
class BatteryStore;
class GpsStore;
class BluetoothStore;
class InternetStore;
class NavigationStore;
class SettingsStore;
class OtaStore;
class UsbStore;
class UmsLogStore;
class SpeedLimitStore;
class AutoStandbyStore;
class CbBatteryStore;
class AuxBatteryStore;
class ThemeStore;
class ScreenStore;
class MenuStore;
class TripStore;
class LocaleStore;
class ShortcutMenuStore;
class ConnectionStore;
class DashboardStore;
class SavedLocationsStore;

class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application();

    bool initialize(QQmlApplicationEngine &engine);
    void fadeInOverlay();
    bool isSimulatorMode() const { return m_simulatorMode; }

private:
    void createStores();
    void createServices();
    void wireSignals();
    void registerQmlSingletons(QQmlApplicationEngine &engine);
    void registerContextProperties(QQmlApplicationEngine &engine);
    void startStoresAndWorker();
    void setupSignalHandlers();

    std::unique_ptr<MdbRepository> m_repository;

    // Stores
    EngineStore *m_engineStore = nullptr;
    VehicleStore *m_vehicleStore = nullptr;
    BatteryStore *m_battery0Store = nullptr;
    BatteryStore *m_battery1Store = nullptr;
    GpsStore *m_gpsStore = nullptr;
    BluetoothStore *m_bluetoothStore = nullptr;
    InternetStore *m_internetStore = nullptr;
    NavigationStore *m_navigationStore = nullptr;
    SettingsStore *m_settingsStore = nullptr;
    OtaStore *m_otaStore = nullptr;
    UsbStore *m_usbStore = nullptr;
    UmsLogStore *m_umsLogStore = nullptr;
    SpeedLimitStore *m_speedLimitStore = nullptr;
    AutoStandbyStore *m_autoStandbyStore = nullptr;
    CbBatteryStore *m_cbBatteryStore = nullptr;
    AuxBatteryStore *m_auxBatteryStore = nullptr;
    ThemeStore *m_themeStore = nullptr;
    ScreenStore *m_screenStore = nullptr;
    MenuStore *m_menuStore = nullptr;
    TripStore *m_tripStore = nullptr;
    ShutdownStore *m_shutdownStore = nullptr;
    LocaleStore *m_localeStore = nullptr;
    ShortcutMenuStore *m_shortcutMenuStore = nullptr;
    ConnectionStore *m_connectionStore = nullptr;
    DashboardStore *m_dashboardStore = nullptr;
    SavedLocationsStore *m_savedLocationsStore = nullptr;

    // Services
    AutoThemeService *m_autoThemeService = nullptr;
    SettingsService *m_settingsService = nullptr;
    NavigationService *m_navigationService = nullptr;
    Translations *m_translations = nullptr;
    InputHandler *m_inputHandler = nullptr;
    ToastService *m_toastService = nullptr;
    MapService *m_mapService = nullptr;
    LowTemperatureMonitor *m_lowTempMonitor = nullptr;
    BluetoothHealthMonitor *m_bleHealthMonitor = nullptr;
    HandlebarLockMonitor *m_handlebarLockMonitor = nullptr;
    NavigationAvailabilityService *m_navAvailability = nullptr;
    SavedLocationsService *m_savedLocationsService = nullptr;
    SerialNumberService *m_serialNumberService = nullptr;
    AddressDatabaseService *m_addressDatabaseService = nullptr;
    SystemInfoService *m_systemInfoService = nullptr;
    SimulatorService *m_simulatorService = nullptr;
    MapDownloadService *m_mapDownloadService = nullptr;
    RoadInfoService *m_roadInfoService = nullptr;

    // Infrastructure
    QSocketNotifier *m_sigTermNotifier = nullptr;
    bool m_simulatorMode = false;
    QList<QObject*> m_stores;
};
