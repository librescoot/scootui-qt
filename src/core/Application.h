#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

class MdbRepository;
class CommandBus;
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
class UpdateChannelService;
class RoadInfoService;
class OdometerMilestoneService;
class MapUpdateCoordinator;
class ReadinessCoordinator;

class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application();

    bool initialize(QQmlApplicationEngine &engine);
    // Call once the first frame is actually on screen. Forwards to
    // ReadinessCoordinator, which hands over the display and gates READY=1.
    void uiPresented();
    bool isSimulatorMode() const { return m_simulatorMode; }
    bool isInMemoryBackend() const { return m_inMemoryBackend; }

private:
    void createStores(QQmlApplicationEngine &engine);
    void registerContextProperties(QQmlApplicationEngine &engine);
    void setupSignalHandlers();
    // Dev aid, off unless SCOOTUI_SCREENSHOT_DIR is set: watches that directory
    // and grabs the dashboard window whenever a file named "request" appears
    // there. The DBC scans out through KMS, so /dev/fb0 shows the console
    // framebuffer rather than the panel and cannot be used to capture the UI.
    void setupScreenshotWatcher();
    void setupSimulatorAutoDrive();

    std::unique_ptr<MdbRepository> m_repository;
    CommandBus *m_commandBus = nullptr;
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
    UpdateChannelService *m_updateChannelService = nullptr;
    RoadInfoService *m_roadInfoService = nullptr;
    OdometerMilestoneService *m_odometerMilestoneService = nullptr;
    MapUpdateCoordinator *m_mapUpdateCoordinator = nullptr;
    ReadinessCoordinator *m_readiness = nullptr;
    bool m_simulatorMode = false;
    bool m_inMemoryBackend = false;
    QString m_backendDescription;
    QList<QObject*> m_stores;
};
