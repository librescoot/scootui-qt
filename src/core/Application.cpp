#include "Application.h"
#include "AppConfig.h"
#include "EnvConfig.h"
#include "repositories/MdbRepository.h"
#include "repositories/InMemoryMdbRepository.h"
#include "repositories/RedisMdbRepository.h"
#include "stores/SyncableStore.h"
#include "stores/EngineStore.h"
#include "stores/VehicleStore.h"
#include "stores/BatteryStore.h"
#include "stores/GpsStore.h"
#include "stores/MotionStore.h"
#include "stores/BluetoothStore.h"
#include "stores/InternetStore.h"
#include "stores/ModemStore.h"
#include "stores/NavigationStore.h"
#include "stores/SettingsStore.h"
#include "stores/OtaStore.h"
#include "stores/UsbStore.h"
#include "stores/UmsLogStore.h"
#include "stores/SpeedLimitStore.h"
#include "stores/AutoStandbyStore.h"
#include "stores/ScooterStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/ThemeStore.h"
#include "core/Navigator.h"
#include "menu/MenuController.h"
#include "services/HopOnService.h"
#include "stores/FaultEventStore.h"
#include "services/FaultsService.h"
#include "services/TripService.h"
#include "stores/ShutdownStore.h"
#include "stores/LocaleStore.h"
#include "menu/ShortcutMenuController.h"
#include "stores/ConnectionStore.h"
#include "stores/DashboardStore.h"
#include "stores/SavedLocationsStore.h"
#include "stores/RecentDestinationsStore.h"
#include "services/SettingsService.h"
#include "services/AutoThemeService.h"
#include "services/InputHandler.h"
#include "services/NavigationService.h"
#include "services/ToastService.h"
#include "services/MapService.h"
#include "services/LowTemperatureMonitor.h"
#include "services/BluetoothHealthMonitor.h"
#include "services/HandlebarLockMonitor.h"
#include "services/BackupBatteryMonitor.h"
#include "services/BatteryAlertModel.h"
#include "services/SystemHealthMonitor.h"
#include "services/FaultNotifier.h"
#include "services/NavigationAvailabilityService.h"
#include "services/SavedLocationsService.h"
#include "services/RecentDestinationsService.h"
#include "services/SerialNumberService.h"
#include "services/AddressDatabaseService.h"
#include "controllers/AddressEntryController.h"
#include "controllers/MapSetupController.h"
#include "coordinators/MapUpdateCoordinator.h"
#include "services/MapDownloadService.h"
#include "services/UpdateChannelService.h"
#include "services/RoadInfoService.h"
#include "services/OdometerMilestoneService.h"
#include "services/SystemInfoService.h"
#include "l10n/Translations.h"
#include "repositories/RedisSchema.h"
#include "commands/CommandBus.h"
#include "simulator/SimulatorService.h"

#include <QQmlContext>
#include <QtQml/qqml.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>

// Shared boot timer defined in main.cpp — markers added at startup
// checkpoints so we can see where time goes on a live DBC.
extern QElapsedTimer g_bootTimer;
#define BOOT_MARK(what) \
    qDebug().nospace().noquote() << QStringLiteral("[boot +%1ms] %2").arg(g_bootTimer.elapsed(), 5).arg(QStringLiteral(what))

// Tiny QObject wrapper exposed to QML so Component.onCompleted handlers
// can log "[boot +Nms]" markers aligned with the C++ BOOT_MARK output.
// Remove alongside the BOOT_MARK call sites once we're done measuring.
class BootTimer : public QObject {
    Q_OBJECT
public:
    explicit BootTimer(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE qint64 elapsed() const { return g_bootTimer.elapsed(); }
};

#ifdef Q_OS_LINUX
#include <QSocketNotifier>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

static int s_sigTermFd[2];

static void sigTermHandler(int)
{
    char a = 1;
    ::write(s_sigTermFd[0], &a, sizeof(a));
}

static void sdNotifyReady()
{
    const char *sockPath = ::getenv("NOTIFY_SOCKET");
    if (!sockPath) return;

    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return;

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);
    if (addr.sun_path[0] == '@')
        addr.sun_path[0] = '\0';

    ::sendto(fd, "READY=1", 7, 0, (struct sockaddr *)&addr,
             offsetof(struct sockaddr_un, sun_path) + ::strlen(sockPath));
    ::close(fd);
}
#endif

Application::Application(QObject *parent)
    : QObject(parent)
{
}

Application::~Application()
{
    // Delete all QObject children before m_repository (unique_ptr) is destroyed.
    // Stores access m_repo in their destructors (unsubscribe, disconnect), so the
    // repo must still be alive when they're deleted.
    const auto children = this->children();
    for (auto *child : children)
        delete child;
}

bool Application::initialize(QQmlApplicationEngine &engine)
{
    // Two independent choices. Which repository backs the UI, and whether the
    // simulator panel runs on top of it. They used to be one: "no Redis host"
    // meant both, which ruled out driving a dedicated Redis from the panel or
    // mirroring a vehicle's.
    const QString redisHost = EnvConfig::redisHost();
    m_inMemoryBackend = redisHost.isEmpty() || redisHost == QLatin1String("none");
    if (m_inMemoryBackend) {
        qDebug() << "Using InMemoryMdbRepository";
        m_repository = std::make_unique<InMemoryMdbRepository>();
        // No local Valhalla behind an in-memory repo, so routing goes online.
        m_repository->set(RedisSchema::hash::Settings,
                          QLatin1String(AppConfig::valhallaEndpointKey),
                          QLatin1String(AppConfig::valhallaOnlineEndpoint));
    } else {
        const int redisPort = EnvConfig::redisPort();
        qDebug() << "Connecting to Redis at" << redisHost << ":" << redisPort;
        m_repository = std::make_unique<RedisMdbRepository>(redisHost, redisPort, QStringLiteral("192.168.8.1"));
    }

    m_backendDescription = m_inMemoryBackend
                         ? QStringLiteral("in-memory")
                         : QStringLiteral("%1:%2").arg(redisHost).arg(EnvConfig::redisPort());
    m_simulatorMode = EnvConfig::simulatorEnabled(m_inMemoryBackend);
    qDebug() << "Simulator panel:" << (m_simulatorMode ? "on" : "off")
             << "backend:" << (m_inMemoryBackend ? QStringLiteral("in-memory") : redisHost);

    qmlRegisterUncreatableMetaObject(ScootEnums::staticMetaObject, "ScootUI", 1, 0, "Scooter", "");
    // Enum-only registrations: the instances stay context properties for now,
    // but QML needs the types to spell HopOnService.Learning etc. instead of
    // hand-mirrored int constants.
    qmlRegisterUncreatableType<HopOnService>("ScootUI", 1, 0, "HopOnService", "enum access only");
    qmlRegisterUncreatableType<AddressDatabaseService>("ScootUI", 1, 0, "AddressDatabaseService", "enum access only");
    qmlRegisterUncreatableType<AddressEntryController>("ScootUI", 1, 0, "AddressEntryController", "enum access only");
    qmlRegisterUncreatableType<MapSetupController>("ScootUI", 1, 0, "MapSetupController", "enum access only");

    BOOT_MARK("createStores() start");
    createStores(engine);
    BOOT_MARK("createStores() done");
    registerContextProperties(engine);
    BOOT_MARK("registerContextProperties() done");
    setupSignalHandlers();
    setupScreenshotWatcher();

    return true;
}

void Application::createStores(QQmlApplicationEngine &engine)
{
    auto *repo = m_repository.get();
    auto *commandBus = new CommandBus(repo, this);
    m_commandBus = commandBus;

    // Core stores (M1)
    auto *engineStore = new EngineStore(repo, this);
    auto *vehicleStore = new VehicleStore(repo, this);
    auto *battery0Store = new BatteryStore(repo, QStringLiteral("0"), this);
    auto *battery1Store = new BatteryStore(repo, QStringLiteral("1"), this);
    auto *gpsStore = new GpsStore(repo, this);
    auto *motionStore = new MotionStore(repo, this);
    auto *bluetoothStore = new BluetoothStore(repo, this);
    auto *internetStore = new InternetStore(repo, this);
    auto *modemStore = new ModemStore(repo, this);
    auto *navigationStore = new NavigationStore(repo, this);
    auto *settingsStore = new SettingsStore(repo, this);
    // The in-memory repository is filled in before the stores exist, so there
    // is no change notification to catch up on. A Redis backend has its sync
    // worker for that. This follows the backend, not the panel.
    if (m_inMemoryBackend)
        settingsStore->refreshAllFields();
    auto *otaStore = new OtaStore(repo, this);
    auto *usbStore = new UsbStore(repo, this);
    auto *umsLogStore = new UmsLogStore(repo, this);
    auto *speedLimitStore = new SpeedLimitStore(repo, this);
    auto *autoStandbyStore = new AutoStandbyStore(repo, this);
    auto *scooterStore = new ScooterStore(repo, this);
    auto *cbBatteryStore = new CbBatteryStore(repo, this);
    auto *auxBatteryStore = new AuxBatteryStore(repo, this);
    auto *themeStore = new ThemeStore(settingsStore, this);
    auto *navigator = new Navigator(settingsStore, commandBus, this);
    auto *tripService = new TripService(engineStore, vehicleStore, this);
    m_shutdownStore = new ShutdownStore(this);
    auto *shutdownStore = m_shutdownStore;
    auto *localeStore = new LocaleStore(settingsStore, this);

    // New stores
    auto *connectionStore = new ConnectionStore(repo, this);
    auto *dashboardStore = new DashboardStore(repo, this);
    BOOT_MARK("stores constructed");

    // M5: Services
    m_settingsService = new SettingsService(repo, settingsStore, this);
    m_translations = new Translations(this);
    m_autoThemeService = new AutoThemeService(repo, themeStore, this);
    m_toastService = new ToastService(this);
    m_serialNumberService = new SerialNumberService(this);
    m_systemInfoService = new SystemInfoService(repo, this);

    // Address database (for destination code lookup). initialize() kicks off
    // a QtConcurrent build job that takes several seconds and competes for
    // CPU with the rest of createStores. Its singleShot is queued after
    // MapService's below, so the map style reload gets the event loop first.
    m_addressDatabaseService = new AddressDatabaseService(this);

    // M7: Navigation service
    m_navigationService = new NavigationService(gpsStore, navigationStore, vehicleStore,
                                                 settingsStore, speedLimitStore, repo, this);

    // Show toast on navigation errors so the user knows what went wrong
    connect(m_navigationService, &NavigationService::errorChanged, this, [this]() {
        QString msg = m_navigationService->errorMessage();
        if (!msg.isEmpty())
            m_toastService->showError(msg);
    });

    // Road info service (extracts street name + speed limit from vector tiles)
    m_roadInfoService = new RoadInfoService(gpsStore, speedLimitStore,
                                             m_navigationService, this);

    // Odometer milestone celebration (500 km, then every 1000 km)
    m_odometerMilestoneService = new OdometerMilestoneService(
        engineStore, vehicleStore, connectionStore, settingsStore, this);

    // Map service (A2)
    m_mapService = new MapService(gpsStore, engineStore, m_navigationService,
                                   settingsStore, themeStore, speedLimitStore,
                                   motionStore, this);

    // Queue AddressDb init now that MapService has queued its own startup
    // reload: we want the map style ready before the trie builder wakes up
    // and starts competing for CPU.
    QTimer::singleShot(0, m_addressDatabaseService, &AddressDatabaseService::initialize);

    // Wire MapService's dead-reckoned position into NavigationService so
    // TBT and off-route detection update smoothly between GPS samples.
    m_navigationService->setMapService(m_mapService);
    // Share the free-drive road match: RoadInfoService selects one segment
    // for metadata, while MapService projects its independent physical pose
    // onto that segment for presentation only.
    m_roadInfoService->setMapService(m_mapService);
    m_mapService->setRoadInfoService(m_roadInfoService);

    // Navigation availability (B6)
    m_navAvailability = new NavigationAvailabilityService(settingsStore, internetStore, repo, this);

    // Map download service. Takes the repository so it can mirror what is
    // installed into the `maps` hash on the MDB.
    m_mapDownloadService = new MapDownloadService(repo, this);
    // Map update lifecycle: auto check/download, DBC power hold, mbtiles
    // reload on install or a late /data mount, scootui:command channel.
    MapUpdateCoordinator::Deps mapDeps;
    mapDeps.repo = repo;
    mapDeps.commandBus = commandBus;
    mapDeps.download = m_mapDownloadService;
    mapDeps.availability = m_navAvailability;
    mapDeps.map = m_mapService;
    mapDeps.roadInfo = m_roadInfoService;
    mapDeps.addressDb = m_addressDatabaseService;
    mapDeps.toasts = m_toastService;
    mapDeps.translations = m_translations;
    mapDeps.gps = gpsStore;
    mapDeps.vehicle = vehicleStore;
    mapDeps.settings = settingsStore;
    mapDeps.internet = internetStore;
    m_mapUpdateCoordinator = new MapUpdateCoordinator(mapDeps, this);

    // Saved locations (B7)
    m_savedLocationsService = new SavedLocationsService(repo, this);
    auto *savedLocationsStore = new SavedLocationsStore(
        repo, m_savedLocationsService, gpsStore, m_roadInfoService,
        m_navigationService, m_toastService, this);

    // Recent destinations — auto-capture every nav request, keep last 10.
    m_recentDestinationsService = new RecentDestinationsService(repo, this);
    auto *recentDestinationsStore = new RecentDestinationsStore(
        repo, m_recentDestinationsService, m_savedLocationsService,
        m_navigationService, m_roadInfoService, m_toastService, this);
    connect(m_navigationService, &NavigationService::destinationRequested,
            recentDestinationsStore, &RecentDestinationsStore::push);

    // Monitoring services (B3, B4)
    m_lowTempMonitor = new LowTemperatureMonitor(engineStore, battery0Store,
                                                   cbBatteryStore, m_toastService, m_translations, this);
    m_bleHealthMonitor = new BluetoothHealthMonitor(bluetoothStore, m_toastService, this);
    m_handlebarLockMonitor = new HandlebarLockMonitor(vehicleStore, m_toastService, m_translations, this);
    m_backupBatteryMonitor = new BackupBatteryMonitor(battery0Store, battery1Store, cbBatteryStore,
                                                       auxBatteryStore, vehicleStore, m_toastService,
                                                       m_translations, this);

    // Debounced status-bar battery warnings (BatteryAlertPolicy conditions).
    auto *batteryAlerts = new BatteryAlertModel(battery0Store, battery1Store, cbBatteryStore,
                                                auxBatteryStore, vehicleStore, this);

    // Battery + ECU fault toasts.
    new FaultNotifier(battery0Store, battery1Store, engineStore,
                      settingsStore, m_toastService, m_translations, this);

    // Wire UMS log polling to USB status
    connect(usbStore, &UsbStore::statusChanged, this, [usbStore, umsLogStore]() {
        const QString &status = usbStore->status();
        if (status == QLatin1String("processing")) {
            umsLogStore->startPolling();
        } else if (status == QLatin1String("idle")) {
            umsLogStore->stopPolling();
            umsLogStore->clear();
        } else {
            umsLogStore->stopPolling();
        }
    });

    // M5: Wire translations to locale
    connect(localeStore, &LocaleStore::languageChanged, m_translations, [this, localeStore]() {
        m_translations->setLanguage(localeStore->language());
    });
    m_translations->setLanguage(localeStore->language());

    // M5: Wire auto-theme to settings
    connect(settingsStore, &SettingsStore::themeChanged, this, [this, settingsStore]() {
        m_autoThemeService->setEnabled(settingsStore->theme() == QLatin1String("auto"));
    });
    if (settingsStore->theme() == QLatin1String("auto")) {
        m_autoThemeService->setEnabled(true);
    }

    // M5: MenuController with full dependencies
    auto *menuController = new MenuController(settingsStore, vehicleStore, themeStore,
                                    m_translations, m_settingsService,
                                    commandBus, this);

    // Wire saved locations, screen store, navigation, and availability into menu
    menuController->setSavedLocationsStore(savedLocationsStore);
    menuController->setRecentDestinationsStore(recentDestinationsStore);
    menuController->setNavigator(navigator);
    menuController->setNavigationService(m_navigationService);
    menuController->setNavigationAvailabilityService(m_navAvailability);
    menuController->setInternetStore(internetStore);

    // Hop-on / hop-off store: combo learning, matching, lock screen.
    auto *hopOnService = new HopOnService(vehicleStore, settingsStore,
                                      m_settingsService,
                                      commandBus, navigator, this);
    menuController->setHopOnService(hopOnService);
    menuController->setMapDownloadService(m_mapDownloadService);

    // Fault aggregation: stream tail + union of per-service active-fault sets.
    auto *faultEventStore = new FaultEventStore(repo, this);
    auto *faultsService = new FaultsService(battery0Store, battery1Store, engineStore,
                                         vehicleStore, bluetoothStore, internetStore,
                                         faultEventStore, m_translations, this);
    menuController->setFaultsService(faultsService);
    menuController->setToastService(m_toastService);

    // Release-channel switching: asks both update-service instances what a
    // switch would download, then applies it on confirmation.
    m_updateChannelService = new UpdateChannelService(m_settingsService, settingsStore,
                                                      otaStore, internetStore,
                                                      m_systemInfoService, this);
    menuController->setUpdateChannelService(m_updateChannelService);

    // Speed up polling while the faults screen is open, slow it back down
    // when it closes so the active-count badge still refreshes without
    // hammering Redis.
    connect(navigator, &Navigator::currentScreenChanged, this,
            [navigator, faultEventStore]() {
        if (navigator->currentScreen() == static_cast<int>(ScootEnums::ScreenMode::Faults))
            faultEventStore->setPollIntervalMs(5000);
        else
            faultEventStore->setPollIntervalMs(30000);
    });
    faultEventStore->start();

    // UMS entry confirmation from the Update Mode info screen. The info
    // screen handles the Back/Start prompt in QML; on Start it emits this
    // signal, and we flip usb:mode so vehicle-service / ums-service kick in.
    connect(navigator, &Navigator::umsModeRequested,
            commandBus, &CommandBus::enterUmsMode);

    // Input handler: sole consumer of vehicle-service's "input-events" stream
    m_inputHandler = new InputHandler(vehicleStore, repo, this);
    menuController->attachInput(m_inputHandler);
    navigator->attachVehicle(vehicleStore);

    // Maintenance-screen gating and connection-loss toasts.
    auto *systemHealth = new SystemHealthMonitor(vehicleStore, connectionStore,
                                                 m_toastService, m_translations, this);

    // Navigation Setup screen policy (MapSetupPolicy over the live stores).
    auto *mapSetup = new MapSetupController(navigator, m_navAvailability, internetStore,
                                            gpsStore, m_mapDownloadService, this);

    // M5: ShortcutMenuController
    auto *shortcutMenuController = new ShortcutMenuController(themeStore, vehicleStore, navigator, dashboardStore, m_inputHandler, commandBus, m_settingsService, this);

    // Address entry: the state machine behind AddressSelectionScreen. Its
    // outcomes route through the same close/resume flow as every other
    // full-screen page.
    auto *addressEntry = new AddressEntryController(m_addressDatabaseService, this);
    connect(addressEntry, &AddressEntryController::dismissed, this,
            [navigator, menuController]() {
        navigator->closeAddressSelection();
        menuController->resume();
    });
    // A chosen destination hands over to the map rather than backing out, so
    // drop the menu level a dismissal would have returned to.
    connect(addressEntry, &AddressEntryController::destinationConfirmed, this,
            [this, navigator, menuController](double lat, double lng, const QString &label) {
        m_navigationService->setDestination(lat, lng, label);
        menuController->close();
        navigator->setScreen(static_cast<int>(ScootEnums::ScreenMode::Map));
    });

    // M6: Wire shutdown to vehicle state monitoring
    m_shutdownStore->connectToVehicle(vehicleStore);

    // Register context properties
    auto *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("bootTimer"), new BootTimer(this));
    ctx->setContextProperty(QStringLiteral("engineStore"), engineStore);
    ctx->setContextProperty(QStringLiteral("vehicleStore"), vehicleStore);
    ctx->setContextProperty(QStringLiteral("battery0Store"), battery0Store);
    ctx->setContextProperty(QStringLiteral("battery1Store"), battery1Store);
    ctx->setContextProperty(QStringLiteral("gpsStore"), gpsStore);
    ctx->setContextProperty(QStringLiteral("motionStore"), motionStore);
    ctx->setContextProperty(QStringLiteral("bluetoothStore"), bluetoothStore);
    ctx->setContextProperty(QStringLiteral("internetStore"), internetStore);
    ctx->setContextProperty(QStringLiteral("modemStore"), modemStore);
    ctx->setContextProperty(QStringLiteral("settingsStore"), settingsStore);
    ctx->setContextProperty(QStringLiteral("otaStore"), otaStore);
    ctx->setContextProperty(QStringLiteral("usbStore"), usbStore);
    ctx->setContextProperty(QStringLiteral("speedLimitStore"), speedLimitStore);
    ctx->setContextProperty(QStringLiteral("autoStandbyStore"), autoStandbyStore);
    ctx->setContextProperty(QStringLiteral("scooterStore"), scooterStore);
    ctx->setContextProperty(QStringLiteral("cbBatteryStore"), cbBatteryStore);
    ctx->setContextProperty(QStringLiteral("auxBatteryStore"), auxBatteryStore);
    ctx->setContextProperty(QStringLiteral("batteryAlerts"), batteryAlerts);
    ctx->setContextProperty(QStringLiteral("themeStore"), themeStore);
    ctx->setContextProperty(QStringLiteral("navigator"), navigator);
    ctx->setContextProperty(QStringLiteral("menuController"), menuController);
    ctx->setContextProperty(QStringLiteral("hopOnService"), hopOnService);
    ctx->setContextProperty(QStringLiteral("tripService"), tripService);
    ctx->setContextProperty(QStringLiteral("shutdownStore"), shutdownStore);
    ctx->setContextProperty(QStringLiteral("shortcutMenuController"), shortcutMenuController);
    ctx->setContextProperty(QStringLiteral("translations"), m_translations);
    ctx->setContextProperty(QStringLiteral("settingsService"), m_settingsService);
    ctx->setContextProperty(QStringLiteral("navigationService"), m_navigationService);

    // New context properties
    ctx->setContextProperty(QStringLiteral("connectionStore"), connectionStore);
    ctx->setContextProperty(QStringLiteral("systemHealth"), systemHealth);
    ctx->setContextProperty(QStringLiteral("mapSetup"), mapSetup);
    ctx->setContextProperty(QStringLiteral("dashboardStore"), dashboardStore);
    ctx->setContextProperty(QStringLiteral("commandBus"), commandBus);
    ctx->setContextProperty(QStringLiteral("toastService"), m_toastService);
    ctx->setContextProperty(QStringLiteral("mapService"), m_mapService);
    ctx->setContextProperty(QStringLiteral("inputHandler"), m_inputHandler);
    ctx->setContextProperty(QStringLiteral("navAvailabilityService"), m_navAvailability);
    ctx->setContextProperty(QStringLiteral("serialNumberService"), m_serialNumberService);
    ctx->setContextProperty(QStringLiteral("addressDatabase"), m_addressDatabaseService);
    ctx->setContextProperty(QStringLiteral("addressEntry"), addressEntry);
    ctx->setContextProperty(QStringLiteral("roadInfoService"), m_roadInfoService);
    ctx->setContextProperty(QStringLiteral("mapDownloadService"), m_mapDownloadService);
    ctx->setContextProperty(QStringLiteral("umsLogStore"), umsLogStore);
    ctx->setContextProperty(QStringLiteral("systemInfoService"), m_systemInfoService);
    ctx->setContextProperty(QStringLiteral("odometerMilestoneService"), m_odometerMilestoneService);
    ctx->setContextProperty(QStringLiteral("faultsService"), faultsService);
    ctx->setContextProperty(QStringLiteral("updateChannelService"), m_updateChannelService);

    // Simulator service (created in sim mode, null otherwise)
    if (m_simulatorMode) {
        // Seed only into the in-memory repository. Against a real Redis the
        // same call would overwrite whatever the vehicle or the services on
        // that instance had already put there, so it becomes a button in the
        // panel instead of something launching the app does to you.
        m_simulatorService = new SimulatorService(repo, m_navigationService,
                                                  m_inMemoryBackend, this);
        ctx->setContextProperty(QStringLiteral("simulator"), m_simulatorService);
        // The panel writes into whatever backs it, so it says what that is.
        ctx->setContextProperty(QStringLiteral("simulatorBackend"), m_backendDescription);
        ctx->setContextProperty(QStringLiteral("simulatorSeeded"), m_inMemoryBackend);
        setupSimulatorAutoDrive();
    } else {
        ctx->setContextProperty(QStringLiteral("simulator"), nullptr);
    }

    // Store references for lifecycle management
    m_stores = {engineStore, vehicleStore, battery0Store, battery1Store,
                gpsStore, motionStore, bluetoothStore, internetStore, modemStore, navigationStore,
                settingsStore, otaStore, usbStore, speedLimitStore,
                autoStandbyStore, scooterStore, cbBatteryStore, auxBatteryStore, dashboardStore};

    BOOT_MARK("services wired");

    // Start all syncable stores (registers their channels with the repo)
    for (auto *store : m_stores) {
        if (auto *syncable = qobject_cast<SyncableStore*>(store)) {
            syncable->start();
        }
    }
    BOOT_MARK("stores started");

    // Register infrequently-polled channels not covered by any store
    repo->registerPollChannel(RedisSchema::hash::System, 30000);
    repo->registerPollChannel(RedisSchema::hash::VersionMdb, 30000);
    repo->registerPollChannel(RedisSchema::hash::VersionDbc, 30000);

    // Synchronous prewarm so QML's first paint sees real values rather than
    // store defaults, eliminating the visible empty-then-populate flash.
    // Capped at 300ms — anything not fetched falls through to the worker
    // thread's normal poll loop. No-op for the in-memory repo (sim mode).
    if (auto *redisRepo = qobject_cast<RedisMdbRepository*>(repo)) {
        redisRepo->prewarmCache(300);
    }
    BOOT_MARK("redis prewarm done");

    // Start the Redis worker thread (after all channels are registered)
    if (auto *redisRepo = qobject_cast<RedisMdbRepository*>(repo)) {
        redisRepo->startWorker();
    }
    BOOT_MARK("redis worker started");

    // Debug: log battery store state after initial sync
    qDebug() << "Battery0 after start: present=" << battery0Store->present()
             << "state=" << battery0Store->batteryState()
             << "charge=" << battery0Store->charge();
    qDebug() << "Battery1 after start: present=" << battery1Store->present()
             << "state=" << battery1Store->batteryState()
             << "charge=" << battery1Store->charge();

    // Notify other services that the dashboard is ready (on startup and every reconnect)
    auto publishReady = [repo, this]() {
        if (m_serialNumberService->available()) {
            repo->set(RedisSchema::hash::Dashboard, QStringLiteral("serial-number"),
                      m_serialNumberService->serialNumber());
        }
        if (m_mapDownloadService)
            m_mapDownloadService->publishToRedis();
        repo->dashboardReady();
        // The first call runs before the worker has connected (the prewarm uses
        // its own throwaway context), so isConnected() is what decides whether
        // the ready publish actually reached Redis.
        m_redisReady = repo->isConnected();
        maybeSignalReady();
    };
    connect(repo, &MdbRepository::connectionStateChanged, this, [publishReady](bool connected) {
        if (connected)
            publishReady();
    });
    BOOT_MARK("publishReady() calling");
    publishReady();
    BOOT_MARK("publishReady() returned");

    qDebug() << "All stores created and started (M5: menu, settings, translations, auto-theme, toast, map, nav-availability, saved-locations, serial-number)";
}




void Application::registerContextProperties(QQmlApplicationEngine &engine)
{
    auto *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("appWidth"), EnvConfig::resolution().width());
    ctx->setContextProperty(QStringLiteral("appHeight"), EnvConfig::resolution().height());
}

void Application::uiPresented()
{
    if (m_uiPresented)
        return;
    m_uiPresented = true;
    fadeInOverlay();
    maybeSignalReady();
}

void Application::maybeSignalReady()
{
#ifdef Q_OS_LINUX
    if (m_readySignalled || !m_uiPresented || !m_redisReady)
        return;
    m_readySignalled = true;
    BOOT_MARK("sd_notify READY=1");
    sdNotifyReady();
#endif
}

void Application::fadeInOverlay()
{
#ifdef Q_OS_LINUX
    auto stopBootAnimation = []() {
        QProcess::startDetached(QStringLiteral("systemctl"),
                                 {QStringLiteral("stop"), QStringLiteral("boot-animation.service")});
        qDebug() << "Boot animation stopped";
    };

    if (!QFile::exists(QStringLiteral("/sys/class/graphics/fb1/overlay_alpha"))) {
        // No overlay alpha (kernel 6.6 imx-drm) — stop boot-animation directly
        stopBootAnimation();
        return;
    }

    qDebug() << "Starting boot animation fade-in...";
    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("/usr/bin/imx-overlay-alpha"));
    proc->setArguments({QStringLiteral("fade"), QStringLiteral("0"),
                        QStringLiteral("255"), QStringLiteral("1000")});

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc, stopBootAnimation](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode == 0) {
            stopBootAnimation();
        }
    });

    proc->start();
#endif
}

void Application::setupSimulatorAutoDrive()
{
    const int route = qEnvironmentVariable("SCOOTUI_SIM_ROUTE").toInt();
    if (route <= 0)
        return;

    // Drives a saved route on startup so navigation and turn behaviour can be
    // watched without clicking through the simulator panel. Both delays wait
    // for the services behind the route, not for anything on screen.
    const double speed = qEnvironmentVariable("SCOOTUI_SIM_AUTODRIVE").toDouble();
    QTimer::singleShot(3000, this, [this, route, speed] {
        if (!m_simulatorService)
            return;
        if (m_settingsService)
            m_settingsService->updateMode(QStringLiteral("navigation"));
        const double timeScale = qEnvironmentVariable("SCOOTUI_SIM_TIMESCALE").toDouble();
        if (timeScale > 0)
            m_simulatorService->setAutoDriveTimeScale(timeScale);
        m_simulatorService->setVehicleState(QStringLiteral("ready-to-drive"));
        m_simulatorService->loadTestRoute(route);
        if (speed > 0) {
            QTimer::singleShot(2000, this, [this, speed] {
                if (m_simulatorService)
                    m_simulatorService->startAutoDrive(speed);
            });
        }
    });
}


void Application::setupScreenshotWatcher()
{
    const QString dir = qEnvironmentVariable("SCOOTUI_SCREENSHOT_DIR");
    if (dir.isEmpty())
        return;

    QDir().mkpath(dir);
    auto *watcher = new QFileSystemWatcher(this);
    if (!watcher->addPath(dir)) {
        qWarning() << "Screenshot watcher: cannot watch" << dir;
        return;
    }

    connect(watcher, &QFileSystemWatcher::directoryChanged, this, [this, dir](const QString &) {
        const QString request = dir + QStringLiteral("/request");
        if (!QFile::exists(request))
            return;
        QFile::remove(request);

        QQuickWindow *window = nullptr;
        const auto windows = QGuiApplication::topLevelWindows();
        for (auto *w : windows) {
            auto *qw = qobject_cast<QQuickWindow*>(w);
            if (qw && qw->title() == QLatin1String("ScootUI")) {
                window = qw;
                break;
            }
        }
        if (!window) {
            qWarning() << "Screenshot watcher: no dashboard window";
            return;
        }

        const QImage img = window->grabWindow();
        const QString path = dir + QStringLiteral("/shot-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
            + QStringLiteral(".png");
        if (img.save(path))
            qDebug() << "Screenshot saved to" << path;
        else
            qWarning() << "Screenshot: failed to save" << path;
    });

    qDebug() << "Screenshot watcher active on" << dir << "(touch 'request' to grab)";
}

void Application::setupSignalHandlers()
{
#ifdef Q_OS_LINUX
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigTermFd) == 0) {
        m_sigTermNotifier = new QSocketNotifier(s_sigTermFd[1],
                                                 QSocketNotifier::Read, this);
        connect(m_sigTermNotifier, &QSocketNotifier::activated, this, [this]() {
            char tmp;
            ::read(s_sigTermFd[1], &tmp, sizeof(tmp));
            qDebug() << "SIGTERM received";
            if (m_shutdownStore) {
                m_shutdownStore->forceBlackout();
            }
            // Hold the black frame for ~2s before exiting. imx-drm does a
            // lastclose/master-release modeset when we exit, which shows up
            // as a visible "no-signal" flash on the DPI panel. The DBC's
            // VBUS is cut 5s after vehicle-service enters ShuttingDown; by
            // waiting 2s we let other DBC services finish and keep the
            // flash hidden behind the power rail going away.
            QTimer::singleShot(2000, &QCoreApplication::quit);
        });

        struct sigaction sa;
        sa.sa_handler = sigTermHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGTERM, &sa, nullptr);

        qDebug() << "SIGTERM handler installed";
    }
#endif
}

// BootTimer's Q_OBJECT class is defined inline above; AUTOMOC needs
// this include to pick it up from the .cpp.
#include "Application.moc"
