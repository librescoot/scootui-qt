#include "Application.h"
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
#include "services/NavigationAvailabilityService.h"
#include "services/SavedLocationsService.h"
#include "services/ReverseGeocodingService.h"
#include "services/SerialNumberService.h"
#include "services/AddressDatabaseService.h"
#include "services/SystemInfoService.h"
#include "l10n/Translations.h"
#include "utils/FaultFormatter.h"
#include "simulator/SimulatorService.h"

#include <QQmlContext>
#include <QDebug>
#include <QProcess>
#include <QFile>

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
    } else {
        qDebug() << "Connecting to Redis at" << redisHost;
        m_repository = std::make_unique<RedisMdbRepository>(redisHost, 6379);
    }

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

    // Address database (for destination code lookup)
    m_addressDatabaseService = new AddressDatabaseService(this);
    m_addressDatabaseService->initialize();

    // M7: Navigation service
    m_navigationService = new NavigationService(gpsStore, navigationStore, vehicleStore,
                                                 settingsStore, speedLimitStore, repo, this);

    // Map service (A2)
    m_mapService = new MapService(gpsStore, engineStore, m_navigationService,
                                   settingsStore, themeStore, this);

    // Navigation availability (B6)
    m_navAvailability = new NavigationAvailabilityService(settingsStore, internetStore, repo, this);

    // Saved locations (B7)
    m_savedLocationsService = new SavedLocationsService(repo, this);
    m_reverseGeocoding = new ReverseGeocodingService(this);
    auto *savedLocationsStore = new SavedLocationsStore(
        m_savedLocationsService, m_reverseGeocoding, gpsStore, m_navigationService,
        m_toastService, this);

    // Monitoring services (B3, B4)
    m_lowTempMonitor = new LowTemperatureMonitor(engineStore, battery0Store,
                                                   cbBatteryStore, m_toastService, this);
    m_bleHealthMonitor = new BluetoothHealthMonitor(bluetoothStore, m_toastService, this);

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
    connectFaultMonitor(battery0Store);
    connectFaultMonitor(battery1Store);

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

    // Wire saved locations, screen store, and navigation into menu
    menuStore->setSavedLocationsStore(savedLocationsStore);
    menuStore->setScreenStore(screenStore);
    menuStore->setNavigationService(m_navigationService);

    // M5: ShortcutMenuStore
    auto *shortcutMenuStore = new ShortcutMenuStore(themeStore, vehicleStore, screenStore, dashboardStore, repo, m_settingsService, this);

    // Input handler: brake gesture detection → menu control
    m_inputHandler = new InputHandler(vehicleStore, menuStore, this);

    // M6: Wire shutdown to vehicle state monitoring
    m_shutdownStore->connectToVehicle(vehicleStore);

    // M6: Handle poweroff request
    connect(m_shutdownStore, &ShutdownStore::requestPoweroff, this, []() {
#ifdef Q_OS_LINUX
        qDebug() << "Executing poweroff...";
        QProcess::startDetached(QStringLiteral("poweroff"), {});
#else
        qDebug() << "Poweroff requested (not on Linux, skipping)";
#endif
    });

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
    ctx->setContextProperty(QStringLiteral("umsLogStore"), umsLogStore);
    ctx->setContextProperty(QStringLiteral("systemInfoService"), m_systemInfoService);

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

    // Start all syncable stores
    for (auto *store : m_stores) {
        if (auto *syncable = qobject_cast<SyncableStore*>(store)) {
            syncable->start();
        }
    }

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
    // Guard: only run if imx overlay alpha interface exists
    if (!QFile::exists(QStringLiteral("/sys/class/graphics/fb1/overlay_alpha")))
        return;

    qDebug() << "Starting boot animation fade-in...";
    auto *proc = new QProcess(this);
    proc->setProgram(QStringLiteral("/usr/bin/imx-overlay-alpha"));
    proc->setArguments({QStringLiteral("fade"), QStringLiteral("0"),
                        QStringLiteral("255"), QStringLiteral("1000")});

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode == 0) {
            QProcess::startDetached(QStringLiteral("systemctl"),
                                     {QStringLiteral("stop"), QStringLiteral("boot-animation.service")});
            qDebug() << "Boot animation stopped";
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
