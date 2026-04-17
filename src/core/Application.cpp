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
#include "stores/BluetoothStore.h"
#include "stores/InternetStore.h"
#include "stores/NavigationStore.h"
#include "stores/SettingsStore.h"
#include "stores/OtaStore.h"
#include "stores/UsbStore.h"
#include "stores/UmsLogStore.h"
#include "stores/SpeedLimitStore.h"
#include "stores/AutoStandbyStore.h"
#include "stores/CbBatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/ThemeStore.h"
#include "stores/ScreenStore.h"
#include "stores/MenuStore.h"
#include "stores/HopOnStore.h"
#include "stores/TripStore.h"
#include "stores/ShutdownStore.h"
#include "stores/LocaleStore.h"
#include "stores/ShortcutMenuStore.h"
#include "stores/ConnectionStore.h"
#include "stores/DashboardStore.h"
#include "stores/SavedLocationsStore.h"
#include "services/SettingsService.h"
#include "services/AutoThemeService.h"
#include "services/InputHandler.h"
#include "services/NavigationService.h"
#include "services/ToastService.h"
#include "services/MapService.h"
#include "services/LowTemperatureMonitor.h"
#include "services/BluetoothHealthMonitor.h"
#include "services/HandlebarLockMonitor.h"
#include "services/NavigationAvailabilityService.h"
#include "services/SavedLocationsService.h"
#include "services/SerialNumberService.h"
#include "services/AddressDatabaseService.h"
#include "services/MapDownloadService.h"
#include "services/RoadInfoService.h"
#include "services/OdometerMilestoneService.h"
#include "services/SystemInfoService.h"
#include "l10n/Translations.h"
#include "utils/FaultFormatter.h"
#include "simulator/SimulatorService.h"

#include <QQmlContext>
#include <QtQml/qqml.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QFile>
#include <QTimer>

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
    const QString redisHost = EnvConfig::redisHost();
    if (redisHost.isEmpty() || redisHost == QLatin1String("none")) {
        qDebug() << "Using InMemoryMdbRepository (simulator mode)";
        m_repository = std::make_unique<InMemoryMdbRepository>();
        m_simulatorMode = true;
        // Use online routing in simulator (no local Valhalla server)
        m_repository->set(QStringLiteral("settings"),
                          QLatin1String(AppConfig::valhallaEndpointKey),
                          QLatin1String(AppConfig::valhallaOnlineEndpoint));
    } else {
        qDebug() << "Connecting to Redis at" << redisHost;
        m_repository = std::make_unique<RedisMdbRepository>(redisHost, 6379, QStringLiteral("192.168.8.1"));
    }

    qmlRegisterUncreatableMetaObject(ScootEnums::staticMetaObject, "ScootUI", 1, 0, "Scooter", "");

    createStores(engine);
    registerContextProperties(engine);
    setupSignalHandlers();

    return true;
}

void Application::createStores(QQmlApplicationEngine &engine)
{
    auto *repo = m_repository.get();

    // Core stores (M1)
    auto *engineStore = new EngineStore(repo, this);
    auto *vehicleStore = new VehicleStore(repo, this);
    auto *battery0Store = new BatteryStore(repo, QStringLiteral("0"), this);
    auto *battery1Store = new BatteryStore(repo, QStringLiteral("1"), this);
    auto *gpsStore = new GpsStore(repo, this);
    auto *bluetoothStore = new BluetoothStore(repo, this);
    auto *internetStore = new InternetStore(repo, this);
    auto *navigationStore = new NavigationStore(repo, this);
    auto *settingsStore = new SettingsStore(repo, this);
    if (m_simulatorMode)
        settingsStore->refreshAllFields();
    auto *otaStore = new OtaStore(repo, this);
    auto *usbStore = new UsbStore(repo, this);
    auto *umsLogStore = new UmsLogStore(repo, this);
    auto *speedLimitStore = new SpeedLimitStore(repo, this);
    auto *autoStandbyStore = new AutoStandbyStore(repo, this);
    auto *cbBatteryStore = new CbBatteryStore(repo, this);
    auto *auxBatteryStore = new AuxBatteryStore(repo, this);
    auto *themeStore = new ThemeStore(settingsStore, this);
    auto *screenStore = new ScreenStore(settingsStore, this);
    auto *tripStore = new TripStore(engineStore, vehicleStore, this);
    m_shutdownStore = new ShutdownStore(this);
    auto *shutdownStore = m_shutdownStore;
    auto *localeStore = new LocaleStore(settingsStore, this);

    // New stores
    auto *connectionStore = new ConnectionStore(repo, this);
    auto *dashboardStore = new DashboardStore(repo, this);

    // M5: Services
    m_settingsService = new SettingsService(repo, this);
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
    m_roadInfoService = new RoadInfoService(gpsStore, speedLimitStore, this);

    // Odometer milestone celebration (500 km, then every 1000 km)
    m_odometerMilestoneService = new OdometerMilestoneService(
        engineStore, vehicleStore, connectionStore, this);

    // Map service (A2)
    m_mapService = new MapService(gpsStore, engineStore, m_navigationService,
                                   settingsStore, themeStore, speedLimitStore, this);

    // Queue AddressDb init now that MapService has queued its own startup
    // reload: we want the map style ready before the trie builder wakes up
    // and starts competing for CPU.
    QTimer::singleShot(0, m_addressDatabaseService, &AddressDatabaseService::initialize);

    // Wire MapService's dead-reckoned position into NavigationService so
    // TBT and off-route detection update smoothly between GPS samples.
    m_navigationService->setMapService(m_mapService);

    // Navigation availability (B6)
    m_navAvailability = new NavigationAvailabilityService(settingsStore, internetStore, repo, this);

    // Map download service
    m_mapDownloadService = new MapDownloadService(m_simulatorMode, this);

    // Auto-check for map updates when connectivity is established
    connect(internetStore, &InternetStore::modemStateChanged, this,
            [this, internetStore, settingsStore]() {
        if (!settingsStore->mapCheckForUpdates())
            return;
        if (internetStore->modemState() != static_cast<int>(ScootEnums::ModemState::Connected))
            return;
        if (!m_mapDownloadService->hasMapsInstalled())
            return;
        if (!m_mapDownloadService->shouldCheckForUpdates())
            return;
        qDebug() << "Auto-checking for map updates (weekly)";
        m_mapDownloadService->checkForUpdates();
    });

    // Notify user when a map update is found, or auto-download if enabled
    connect(m_mapDownloadService, &MapDownloadService::updateAvailableChanged, this,
            [this, settingsStore, gpsStore]() {
        if (!m_mapDownloadService->updateAvailable())
            return;
        if (settingsStore->mapAutoDownload()) {
            qDebug() << "Auto-downloading map update";
            m_mapDownloadService->startDownload(
                gpsStore->latitude(), gpsStore->longitude(), true, true);
        } else {
            m_toastService->showInfo(m_translations->mapUpdateAvailableToast());
        }
    });

    // Show persisted update notification on startup while parked/stand-by
    if (m_mapDownloadService->updateAvailable()) {
        auto *startupConn = new QMetaObject::Connection;
        *startupConn = connect(vehicleStore, &VehicleStore::stateChanged, this,
                [this, vehicleStore, startupConn]() {
            auto state = static_cast<ScootEnums::VehicleState>(vehicleStore->state());
            if (state == ScootEnums::VehicleState::Parked
                || state == ScootEnums::VehicleState::StandBy) {
                if (m_mapDownloadService->updateAvailable())
                    m_toastService->showInfo(m_translations->mapUpdateAvailableToast());
            }
            disconnect(*startupConn);
            delete startupConn;
        });
    }

    // Watch /data/maps/ for mbtiles appearing late (e.g. /data not yet
    // mounted at startup) or being replaced (e.g. OTA map update).
    // inotify on the mountpoint directory fires when the filesystem is mounted.
    auto *mbtilesWatcher = new QFileSystemWatcher(this);
    static const QString mapsDir = QStringLiteral("/data/maps");
    if (QDir(mapsDir).exists())
        mbtilesWatcher->addPath(mapsDir);
    else
        mbtilesWatcher->addPath(QStringLiteral("/data"));

    connect(mbtilesWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this, mbtilesWatcher](const QString &path) {
        static const QString mapsDir = QStringLiteral("/data/maps");
        // /data was just mounted — start watching /data/maps/ instead
        if (path == QLatin1String("/data") && QDir(mapsDir).exists()) {
            mbtilesWatcher->removePath(QStringLiteral("/data"));
            mbtilesWatcher->addPath(mapsDir);
        }
        if (!QFile::exists(mapsDir + QStringLiteral("/map.mbtiles")))
            return;
        qDebug() << "Mbtiles change detected, reloading services";
        m_mapService->reloadMbtiles();
        m_roadInfoService->reloadMbtiles();
        m_addressDatabaseService->initialize();
    });

    // Saved locations (B7)
    m_savedLocationsService = new SavedLocationsService(repo, this);
    auto *savedLocationsStore = new SavedLocationsStore(
        repo, m_savedLocationsService, gpsStore, m_roadInfoService,
        m_navigationService, m_toastService, this);

    // Monitoring services (B3, B4)
    m_lowTempMonitor = new LowTemperatureMonitor(engineStore, battery0Store,
                                                   cbBatteryStore, m_toastService, m_translations, this);
    m_bleHealthMonitor = new BluetoothHealthMonitor(bluetoothStore, m_toastService, this);
    m_handlebarLockMonitor = new HandlebarLockMonitor(vehicleStore, m_toastService, m_translations, this);

    // Battery fault monitoring
    auto connectFaultMonitor = [this, settingsStore](BatteryStore *batteryStore) {
        connect(batteryStore, &BatteryStore::faultsChanged, this,
                [this, batteryStore, settingsStore]() {
            auto faults = batteryStore->faults();
            if (faults.isEmpty())
                return;
            // Suppress battery 1 fault toasts unless dual battery mode is enabled
            if (batteryStore->batteryId() != QLatin1String("0") && !settingsStore->dualBattery())
                return;
            QString slotName = batteryStore->batteryId() == QLatin1String("0")
                ? m_translations->batterySlot0()
                : m_translations->batterySlot1();
            if (faults.size() == 1) {
                QString msg = slotName + QStringLiteral(": ")
                    + FaultFormatter::formatSingleFault(faults.first(), m_translations);
                if (FaultFormatter::hasAnyCritical(faults))
                    m_toastService->showError(msg);
                else
                    m_toastService->showWarning(msg);
            } else {
                QString title = slotName + QStringLiteral(": ")
                    + FaultFormatter::getMultipleFaultsTitle(faults, m_translations);
                QString detail = FaultFormatter::formatMultipleFaults(faults, m_translations);
                if (FaultFormatter::hasAnyCritical(faults))
                    m_toastService->showError(title + QStringLiteral("\n") + detail);
                else
                    m_toastService->showWarning(title + QStringLiteral("\n") + detail);
            }
        });
    };
    connectFaultMonitor(battery0Store);
    connectFaultMonitor(battery1Store);

    // ECU fault monitoring — mirrors the battery pattern but with "E" prefix and
    // different code→description mapping. Single-code, no fault set.
    connect(engineStore, &EngineStore::faultCodeChanged, this, [this, engineStore]() {
        int code = engineStore->faultCode();
        if (code == 0)
            return;
        QString msg = FaultFormatter::formatEcuFault(code, m_translations);
        if (FaultFormatter::getEcuSeverity(code) == FaultSeverity::Critical)
            m_toastService->showError(msg);
        else
            m_toastService->showWarning(msg);
    });

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

    // M5: MenuStore with full dependencies
    auto *menuStore = new MenuStore(settingsStore, vehicleStore, themeStore,
                                    tripStore, m_translations, m_settingsService,
                                    repo, this);

    // Wire saved locations, screen store, navigation, and availability into menu
    menuStore->setSavedLocationsStore(savedLocationsStore);
    menuStore->setScreenStore(screenStore);
    menuStore->setNavigationService(m_navigationService);
    menuStore->setNavigationAvailabilityService(m_navAvailability);
    menuStore->setInternetStore(internetStore);

    // Hop-on / hop-off store: combo learning, matching, lock screen.
    auto *hopOnStore = new HopOnStore(vehicleStore, settingsStore,
                                      m_settingsService, dashboardStore,
                                      repo, this);
    menuStore->setHopOnStore(hopOnStore);
    menuStore->setMapDownloadService(m_mapDownloadService);

    // M5: ShortcutMenuStore
    auto *shortcutMenuStore = new ShortcutMenuStore(themeStore, vehicleStore, screenStore, dashboardStore, repo, m_settingsService, this);

    // Input handler: consumes vehicle-service's "input-events" gesture stream
    m_inputHandler = new InputHandler(vehicleStore, repo, this);

    // M6: Wire shutdown to vehicle state monitoring
    m_shutdownStore->connectToVehicle(vehicleStore);

    // Register context properties
    auto *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("engineStore"), engineStore);
    ctx->setContextProperty(QStringLiteral("vehicleStore"), vehicleStore);
    ctx->setContextProperty(QStringLiteral("battery0Store"), battery0Store);
    ctx->setContextProperty(QStringLiteral("battery1Store"), battery1Store);
    ctx->setContextProperty(QStringLiteral("gpsStore"), gpsStore);
    ctx->setContextProperty(QStringLiteral("bluetoothStore"), bluetoothStore);
    ctx->setContextProperty(QStringLiteral("internetStore"), internetStore);
    ctx->setContextProperty(QStringLiteral("navigationStore"), navigationStore);
    ctx->setContextProperty(QStringLiteral("settingsStore"), settingsStore);
    ctx->setContextProperty(QStringLiteral("otaStore"), otaStore);
    ctx->setContextProperty(QStringLiteral("usbStore"), usbStore);
    ctx->setContextProperty(QStringLiteral("speedLimitStore"), speedLimitStore);
    ctx->setContextProperty(QStringLiteral("autoStandbyStore"), autoStandbyStore);
    ctx->setContextProperty(QStringLiteral("cbBatteryStore"), cbBatteryStore);
    ctx->setContextProperty(QStringLiteral("auxBatteryStore"), auxBatteryStore);
    ctx->setContextProperty(QStringLiteral("themeStore"), themeStore);
    ctx->setContextProperty(QStringLiteral("screenStore"), screenStore);
    ctx->setContextProperty(QStringLiteral("menuStore"), menuStore);
    ctx->setContextProperty(QStringLiteral("hopOnStore"), hopOnStore);
    ctx->setContextProperty(QStringLiteral("tripStore"), tripStore);
    ctx->setContextProperty(QStringLiteral("shutdownStore"), shutdownStore);
    ctx->setContextProperty(QStringLiteral("localeStore"), localeStore);
    ctx->setContextProperty(QStringLiteral("shortcutMenuStore"), shortcutMenuStore);
    ctx->setContextProperty(QStringLiteral("translations"), m_translations);
    ctx->setContextProperty(QStringLiteral("settingsService"), m_settingsService);
    ctx->setContextProperty(QStringLiteral("navigationService"), m_navigationService);

    // New context properties
    ctx->setContextProperty(QStringLiteral("connectionStore"), connectionStore);
    ctx->setContextProperty(QStringLiteral("dashboardStore"), dashboardStore);
    ctx->setContextProperty(QStringLiteral("toastService"), m_toastService);
    ctx->setContextProperty(QStringLiteral("mapService"), m_mapService);
    ctx->setContextProperty(QStringLiteral("inputHandler"), m_inputHandler);
    ctx->setContextProperty(QStringLiteral("navAvailabilityService"), m_navAvailability);
    ctx->setContextProperty(QStringLiteral("savedLocationsStore"), savedLocationsStore);
    ctx->setContextProperty(QStringLiteral("serialNumberService"), m_serialNumberService);
    ctx->setContextProperty(QStringLiteral("addressDatabase"), m_addressDatabaseService);
    ctx->setContextProperty(QStringLiteral("mapDownloadService"), m_mapDownloadService);
    ctx->setContextProperty(QStringLiteral("umsLogStore"), umsLogStore);
    ctx->setContextProperty(QStringLiteral("systemInfoService"), m_systemInfoService);
    ctx->setContextProperty(QStringLiteral("odometerMilestoneService"), m_odometerMilestoneService);

    // Simulator service (created in sim mode, null otherwise)
    if (m_simulatorMode) {
        m_simulatorService = new SimulatorService(repo, m_navigationService, this);
        ctx->setContextProperty(QStringLiteral("simulator"), m_simulatorService);
        ctx->setContextProperty(QStringLiteral("simulatorMode"), true);
    } else {
        ctx->setContextProperty(QStringLiteral("simulator"), nullptr);
        ctx->setContextProperty(QStringLiteral("simulatorMode"), false);
    }

    // Store references for lifecycle management
    m_stores = {engineStore, vehicleStore, battery0Store, battery1Store,
                gpsStore, bluetoothStore, internetStore, navigationStore,
                settingsStore, otaStore, usbStore, speedLimitStore,
                autoStandbyStore, cbBatteryStore, auxBatteryStore, dashboardStore};

    // Start all syncable stores (registers their channels with the repo)
    for (auto *store : m_stores) {
        if (auto *syncable = qobject_cast<SyncableStore*>(store)) {
            syncable->start();
        }
    }

    // Register infrequently-polled channels not covered by any store
    repo->registerPollChannel(QStringLiteral("system"), 30000);
    repo->registerPollChannel(QStringLiteral("version:mdb"), 30000);
    repo->registerPollChannel(QStringLiteral("version:dbc"), 30000);

    // Start the Redis worker thread (after all channels are registered)
    if (auto *redisRepo = qobject_cast<RedisMdbRepository*>(repo)) {
        redisRepo->startWorker();
    }

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
            repo->set(QStringLiteral("dashboard"), QStringLiteral("serial-number"),
                      m_serialNumberService->serialNumber());
        }
        repo->dashboardReady();
#ifdef Q_OS_LINUX
        // Only signal systemd once — subsequent calls are reconnect events.
        static bool notified = false;
        if (!notified) {
            sdNotifyReady();
            notified = true;
        }
#endif
    };
    connect(repo, &MdbRepository::connectionStateChanged, this, [publishReady](bool connected) {
        if (connected)
            publishReady();
    });
    publishReady();

    qDebug() << "All stores created and started (M5: menu, settings, translations, auto-theme, toast, map, nav-availability, saved-locations, serial-number)";
}

void Application::registerContextProperties(QQmlApplicationEngine &engine)
{
    auto *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("appWidth"), EnvConfig::resolution().width());
    ctx->setContextProperty(QStringLiteral("appHeight"), EnvConfig::resolution().height());
    ctx->setContextProperty(QStringLiteral("scaleFactor"), EnvConfig::scaleFactor());
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
            // Blackout fade is 600ms; quit just after so systemd doesn't
            // spend the rest of its TimeoutStopSec waiting to SIGKILL us.
            QTimer::singleShot(700, &QCoreApplication::quit);
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
