#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

class MdbRepository;
class GpsStore;
class VehicleStore;
class SettingsStore;
class InternetStore;
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

class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application();

    bool initialize(QQmlApplicationEngine &engine);
    // Call once the first frame is actually on screen. Hands the display over
    // from boot-animation and, together with the Redis connect, gates the
    // systemd READY=1. Idempotent.
    void uiPresented();
    bool isSimulatorMode() const { return m_simulatorMode; }
    bool isInMemoryBackend() const { return m_inMemoryBackend; }

private:
    void fadeInOverlay();
    // READY=1 fires once both halves are true: the UI has painted and the
    // Redis worker is connected (so the dashboard hash has really been
    // published). Called from both edges, whichever lands last wins.
    void maybeSignalReady();
    void createStores(QQmlApplicationEngine &engine);
    void registerContextProperties(QQmlApplicationEngine &engine);
    void setupSignalHandlers();
    // Dev aid, off unless SCOOTUI_SCREENSHOT_DIR is set: watches that directory
    // and grabs the dashboard window whenever a file named "request" appears
    // there. The DBC scans out through KMS, so /dev/fb0 shows the console
    // framebuffer rather than the panel and cannot be used to capture the UI.
    void setupScreenshotWatcher();
    void setupSimulatorAutoDrive();
    void setupMapCommandChannel();
    // Re-point the mbtiles-backed services at /data/maps/map.mbtiles. Safe to
    // call repeatedly (each service's reload is idempotent); driven by the
    // file watcher and by NavigationAvailabilityService::localMapsBecameAvailable
    // so a late /data mount recovers the map + road-info, not just the flag.
    void reloadMapServices();
    // Starts the map download only while parked/stand-by, so a mid-ride
    // update never triggers a large cellular download or a valhalla restart
    // during navigation. Self-gates on the auto-download setting, update
    // availability, download-service idle status, and vehicle state, so it's
    // safe to call opportunistically from multiple signals.
    void maybeAutoDownloadMaps();
    // Runs the periodic map update check when the setting, connectivity,
    // installed maps and cadence all allow it. Cheap and idempotent, so it is
    // safe to call from every signal that could make those conditions true.
    void maybeCheckForMapUpdates();

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
    UpdateChannelService *m_updateChannelService = nullptr;
    RoadInfoService *m_roadInfoService = nullptr;
    OdometerMilestoneService *m_odometerMilestoneService = nullptr;
    // Stashed for maybeAutoDownloadMaps(), which needs to be reachable from
    // several connects (updateAvailableChanged, vehicleStore::stateChanged,
    // and the startup check) without duplicating its gating logic in each.
    GpsStore *m_gpsStore = nullptr;
    VehicleStore *m_vehicleStore = nullptr;
    SettingsStore *m_settingsStore = nullptr;
    InternetStore *m_internetStore = nullptr;
    bool m_simulatorMode = false;
    bool m_inMemoryBackend = false;
    QString m_backendDescription;
    quint64 m_mapCommandSubscriptionId = 0;
    bool m_mapDownloadHoldActive = false;
    bool m_uiPresented = false;
    bool m_redisReady = false;
    bool m_readySignalled = false;
    QList<QObject*> m_stores;
};
