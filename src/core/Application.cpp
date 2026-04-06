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
#include "services/SystemInfoService.h"
#include "l10n/Translations.h"
#include "utils/FaultFormatter.h"
#include "simulator/SimulatorService.h"

#include <QQmlContext>
#include <QDebug>
#include <QProcess>
#include <QFile>
#include <QTimer>

#ifdef Q_OS_LINUX
#include <QSocketNotifier>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

static int s_sigTermFd[2];

static void sigTermHandler(int)
{
    char a = 1;
    ::write(s_sigTermFd[0], &a, sizeof(a));
}
#endif

Application::Application(QObject *parent)
    : QObject(parent)
{
}

Application::~Application() = default;

bool Application::initialize(QQmlApplicationEngine &engine)
{
    const QString redisHost = EnvConfig::redisHost();
    if (redisHost.isEmpty() || redisHost == QLatin1String("none")) {
        qDebug() << "Using InMemoryMdbRepository (simulator mode)";
        m_repository = std::make_unique<InMemoryMdbRepository>();
        m_simulatorMode = true;
        m_repository->set(QStringLiteral("settings"),
                          QLatin1String(AppConfig::valhallaEndpointKey),
                          QLatin1String(AppConfig::valhallaOnlineEndpoint));
    } else {
        qDebug() << "Connecting to Redis at" << redisHost;
        m_repository = std::make_unique<RedisMdbRepository>(redisHost, 6379, QStringLiteral("192.168.8.1"));
    }

    createStores();
    createServices();
    wireSignals();
    registerQmlSingletons(engine);
    registerContextProperties(engine);
    startStoresAndWorker();
    setupSignalHandlers();

    return true;
}

void Application::createStores()
{
    auto *repo = m_repository.get();

    m_engineStore = new EngineStore(repo, this);
    m_vehicleStore = new VehicleStore(repo, this);
    m_battery0Store = new BatteryStore(repo, QStringLiteral("0"), this);
    m_battery1Store = new BatteryStore(repo, QStringLiteral("1"), this);
    Battery0StoreForeign::s_instance = m_battery0Store;
    Battery1StoreForeign::s_instance = m_battery1Store;
    m_gpsStore = new GpsStore(repo, this);
    m_bluetoothStore = new BluetoothStore(repo, this);
    m_internetStore = new InternetStore(repo, this);
    m_navigationStore = new NavigationStore(repo, this);
    m_settingsStore = new SettingsStore(repo, this);
    if (m_simulatorMode)
        m_settingsStore->refreshAllFields();
    m_otaStore = new OtaStore(repo, this);
    m_usbStore = new UsbStore(repo, this);
    m_umsLogStore = new UmsLogStore(repo, this);
    m_speedLimitStore = new SpeedLimitStore(repo, this);
    m_autoStandbyStore = new AutoStandbyStore(repo, this);
    m_cbBatteryStore = new CbBatteryStore(repo, this);
    m_auxBatteryStore = new AuxBatteryStore(repo, this);
    m_themeStore = new ThemeStore(m_settingsStore, this);
    m_screenStore = new ScreenStore(m_settingsStore, this);
    m_tripStore = new TripStore(m_engineStore, m_vehicleStore, this);
    m_shutdownStore = new ShutdownStore(this);
    m_localeStore = new LocaleStore(m_settingsStore, this);
    m_connectionStore = new ConnectionStore(repo, this);
    m_dashboardStore = new DashboardStore(repo, this);
}

void Application::createServices()
{
    auto *repo = m_repository.get();

    m_settingsService = new SettingsService(repo, this);
    m_translations = new Translations(this);
    m_autoThemeService = new AutoThemeService(repo, m_themeStore, this);
    m_toastService = new ToastService(this);
    m_serialNumberService = new SerialNumberService(this);
    m_systemInfoService = new SystemInfoService(repo, this);

    m_addressDatabaseService = new AddressDatabaseService(this);
    m_addressDatabaseService->initialize();

    m_navigationService = new NavigationService(m_gpsStore, m_navigationStore, m_vehicleStore,
                                                 m_settingsStore, m_speedLimitStore, repo, this);

    m_roadInfoService = new RoadInfoService(m_gpsStore, m_speedLimitStore, this);

    m_mapService = new MapService(m_gpsStore, m_engineStore, m_navigationService,
                                   m_settingsStore, m_themeStore, m_speedLimitStore, this);

    m_navAvailability = new NavigationAvailabilityService(m_settingsStore, m_internetStore, repo, this);

    m_mapDownloadService = new MapDownloadService(m_simulatorMode, this);

    m_savedLocationsService = new SavedLocationsService(repo, this);
    m_savedLocationsStore = new SavedLocationsStore(
        repo, m_savedLocationsService, m_gpsStore, m_roadInfoService,
        m_navigationService, m_toastService, this);

    m_lowTempMonitor = new LowTemperatureMonitor(m_engineStore, m_battery0Store,
                                                   m_cbBatteryStore, m_toastService, this);
    m_bleHealthMonitor = new BluetoothHealthMonitor(m_bluetoothStore, m_toastService, this);
    m_handlebarLockMonitor = new HandlebarLockMonitor(m_vehicleStore, m_toastService, this);

    m_menuStore = new MenuStore(m_settingsStore, m_vehicleStore, m_themeStore,
                                m_tripStore, m_translations, m_settingsService,
                                repo, this);
    m_menuStore->setSavedLocationsStore(m_savedLocationsStore);
    m_menuStore->setScreenStore(m_screenStore);
    m_menuStore->setNavigationService(m_navigationService);
    m_menuStore->setNavigationAvailabilityService(m_navAvailability);
    m_menuStore->setInternetStore(m_internetStore);

    m_shortcutMenuStore = new ShortcutMenuStore(m_themeStore, m_vehicleStore, m_screenStore,
                                                m_dashboardStore, repo, m_settingsService, this);

    m_inputHandler = new InputHandler(m_vehicleStore, m_menuStore, this);

    if (m_simulatorMode)
        m_simulatorService = new SimulatorService(repo, m_navigationService, this);
}

void Application::wireSignals()
{
    // Navigation error toasts
    connect(m_navigationService, &NavigationService::errorChanged, this, [this]() {
        QString msg = m_navigationService->errorMessage();
        if (!msg.isEmpty())
            m_toastService->showError(msg);
    });

    // Battery fault monitoring
    auto connectFaultMonitor = [this](BatteryStore *batteryStore) {
        connect(batteryStore, &BatteryStore::faultsChanged, this,
                [this, batteryStore]() {
            auto faults = batteryStore->faults();
            if (faults.isEmpty())
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
    connectFaultMonitor(m_battery0Store);
    connectFaultMonitor(m_battery1Store);

    // UMS log polling driven by USB status
    connect(m_usbStore, &UsbStore::statusChanged, this, [this]() {
        const QString &status = m_usbStore->status();
        if (status == QLatin1String("processing")) {
            m_umsLogStore->startPolling();
        } else if (status == QLatin1String("idle")) {
            m_umsLogStore->stopPolling();
            m_umsLogStore->clear();
        } else {
            m_umsLogStore->stopPolling();
        }
    });

    // Translations follow locale
    connect(m_localeStore, &LocaleStore::languageChanged, m_translations, [this]() {
        m_translations->setLanguage(m_localeStore->language());
    });
    m_translations->setLanguage(m_localeStore->language());

    // Auto-theme follows settings
    connect(m_settingsStore, &SettingsStore::themeChanged, this, [this]() {
        m_autoThemeService->setEnabled(m_settingsStore->theme() == QLatin1String("auto"));
    });
    if (m_settingsStore->theme() == QLatin1String("auto")) {
        m_autoThemeService->setEnabled(true);
    }

    // Shutdown reacts to vehicle state
    m_shutdownStore->connectToVehicle(m_vehicleStore);
}

void Application::registerQmlSingletons(QQmlApplicationEngine &engine)
{
    auto *ctx = engine.rootContext();
    if (m_simulatorMode) {
        ctx->setContextProperty(QStringLiteral("simulator"), m_simulatorService);
        ctx->setContextProperty(QStringLiteral("simulatorMode"), true);
    } else {
        ctx->setContextProperty(QStringLiteral("simulator"), nullptr);
        ctx->setContextProperty(QStringLiteral("simulatorMode"), false);
    }
}

void Application::startStoresAndWorker()
{
    auto *repo = m_repository.get();

    m_stores = {m_engineStore, m_vehicleStore, m_battery0Store, m_battery1Store,
                m_gpsStore, m_bluetoothStore, m_internetStore, m_navigationStore,
                m_settingsStore, m_otaStore, m_usbStore, m_speedLimitStore,
                m_autoStandbyStore, m_cbBatteryStore, m_auxBatteryStore, m_dashboardStore};

    for (auto *store : m_stores) {
        if (auto *syncable = qobject_cast<SyncableStore*>(store)) {
            syncable->start();
        }
    }

    repo->registerPollChannel(QStringLiteral("system"), 30000);
    repo->registerPollChannel(QStringLiteral("version:mdb"), 30000);
    repo->registerPollChannel(QStringLiteral("version:dbc"), 30000);

    if (auto *redisRepo = qobject_cast<RedisMdbRepository*>(repo)) {
        redisRepo->startWorker();
    }

    auto publishReady = [repo, this]() {
        if (m_serialNumberService->available()) {
            repo->set(QStringLiteral("dashboard"), QStringLiteral("serial-number"),
                      m_serialNumberService->serialNumber());
        }
        repo->dashboardReady();
    };
    connect(repo, &MdbRepository::connectionStateChanged, this, [publishReady](bool connected) {
        if (connected)
            publishReady();
    });
    publishReady();
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
