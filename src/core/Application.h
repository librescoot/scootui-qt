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
class BackupBatteryMonitor;
class NavigationAvailabilityService;
class SavedLocationsService;
class RecentDestinationsService;
class SerialNumberService;
class AddressDatabaseService;
class SystemInfoService;
class SimulatorService;
class MapDownloadService;
class RoadInfoService;
class OdometerMilestoneService;

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
    void createStores(QQmlApplicationEngine &engine);
    void registerContextProperties(QQmlApplicationEngine &engine);
    void setupSignalHandlers();
    // Re-point the mbtiles-backed services at /data/maps/map.mbtiles. Safe to
    // call repeatedly (each service's reload is idempotent); driven by the
    // file watcher and by NavigationAvailabilityService::localMapsBecameAvailable
    // so a late /data mount recovers the map + road-info, not just the flag.
    void reloadMapServices();

    std::unique_ptr<MdbRepository> m_repository;
    AutoThemeService *m_autoThemeService = nullptr;
    SettingsService *m_settingsService = nullptr;
    NavigationService *m_navigationService = nullptr;
    Translations *m_translations = nullptr;
    InputHandler *m_inputHandler = nullptr;
    ShutdownStore *m_shutdownStore = nullptr;
    QSocketNotifier *m_sigTermNotifier = nullptr;
    ToastService *m_toastService = nullptr;
    MapService *m_mapService = nullptr;
    LowTemperatureMonitor *m_lowTempMonitor = nullptr;
    BluetoothHealthMonitor *m_bleHealthMonitor = nullptr;
    HandlebarLockMonitor *m_handlebarLockMonitor = nullptr;
    BackupBatteryMonitor *m_backupBatteryMonitor = nullptr;
    NavigationAvailabilityService *m_navAvailability = nullptr;
    SavedLocationsService *m_savedLocationsService = nullptr;
    RecentDestinationsService *m_recentDestinationsService = nullptr;
    SerialNumberService *m_serialNumberService = nullptr;
    AddressDatabaseService *m_addressDatabaseService = nullptr;
    SystemInfoService *m_systemInfoService = nullptr;
    SimulatorService *m_simulatorService = nullptr;
    MapDownloadService *m_mapDownloadService = nullptr;
    RoadInfoService *m_roadInfoService = nullptr;
    OdometerMilestoneService *m_odometerMilestoneService = nullptr;
    bool m_simulatorMode = false;
    QList<QObject*> m_stores;
};
