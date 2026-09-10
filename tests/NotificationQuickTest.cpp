#include <QtQuickTest/quicktest.h>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include "services/NavigationService.h"
#include "services/NotificationService.h"
#include "services/NotificationIngress.h"
#include "services/ToastService.h"
#include "services/BackupBatteryMonitor.h"
#include "stores/BatteryStore.h"
#include "stores/AuxBatteryStore.h"
#include "stores/CbBatteryStore.h"
#include "repositories/InMemoryMdbRepository.h"
#include "routing/RouteHelpers.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"
#include "l10n/Translations.h"

class NativeAttentionHarness : public QObject
{
    Q_OBJECT
    Q_PROPERTY(NotificationService *notifications READ notifications CONSTANT)
    Q_PROPERTY(ToastService *toasts READ toasts CONSTANT)
    Q_PROPERTY(NavigationService *navigation READ navigation CONSTANT)
    Q_PROPERTY(Translations *translations READ translations CONSTANT)
    Q_PROPERTY(VehicleStore *vehicle READ vehicle CONSTANT)
    Q_PROPERTY(int arrivalEvents READ arrivalEvents NOTIFY arrivalEventsChanged)

public:
    NativeAttentionHarness()
        : m_gps(&m_repo), m_navStore(&m_repo), m_vehicle(&m_repo), m_settings(&m_repo),
          m_speedLimit(&m_repo), m_battery0(&m_repo, "0"), m_battery1(&m_repo, "1"),
          m_auxBattery(&m_repo), m_cbBattery(&m_repo), m_notifications(false, nullptr, [this] { return m_now; }),
          m_ingress(&m_repo, &m_notifications)
    {
        // Keep route requests pending locally so loading can be rendered deterministically.
        if (!m_router.listen(QHostAddress::LocalHost))
            qFatal("Could not start the local route fixture endpoint");
        m_repo.set("settings", "dashboard.valhalla-url",
                   QStringLiteral("http://127.0.0.1:%1").arg(m_router.serverPort()));
        m_repo.set("vehicle", "state", "ready-to-drive");
        m_repo.set("battery:0", "present", "true");
        m_settings.start();
        m_battery0.start();
        m_battery1.start();
        m_auxBattery.start();
        m_cbBattery.start();
        m_vehicle.start();
        m_gps.start();
        m_navStore.start();
        m_navigation = new NavigationService(&m_gps, &m_navStore, &m_vehicle, &m_settings,
                                             &m_speedLimit, &m_repo, this);
        m_notifications.setVehicleStore(&m_vehicle);
        m_toasts.setNotificationService(&m_notifications);
        m_backupMonitor = new BackupBatteryMonitor(&m_battery0, &m_battery1, &m_cbBattery,
            &m_auxBattery, &m_vehicle, &m_toasts, &m_translations, this);
        const auto refresh = [this]() {
            if (m_navigation->status() == 0) {
                m_notifications.setNavigationPayload({});
                return;
            }
            m_notifications.setNavigationPayload({
                {"id", "navigation-session"}, {"valid", true}, {"status", m_navigation->status()},
                {"maneuverType", m_navigation->currentManeuverType()},
                {"instruction", m_navigation->currentVerbalInstruction()},
                {"compactInstruction", m_navigation->currentCompactInstruction()},
                {"distance", m_navigation->currentManeuverDistance()},
                {"isStart", m_navigation->currentIsStart()},
                {"distanceToDestination", m_navigation->distanceToDestination()},
                {"remainingDuration", m_navigation->remainingDuration()}, {"eta", m_navigation->eta()}});
        };
        connect(m_navigation, &NavigationService::statusChanged, this, refresh);
        connect(m_navigation, &NavigationService::instructionChanged, this, refresh);
        connect(m_navigation, &NavigationService::positionChanged, this, refresh);
        connect(m_navigation, &NavigationService::arrived, this, [this]() {
            ++m_arrivalEvents;
            emit arrivalEventsChanged();
            m_notifications.publishEvent("navigation-arrived", "navigation", m_translations.navArrived(),
                                         {}, 3, "success", 10000);
        });
        connect(m_navigation, &NavigationService::arrivalReset, this, [this]() {
            m_notifications.clearEvent("navigation-arrived");
        });
    }

    ~NativeAttentionHarness() override { delete m_backupMonitor; delete m_navigation; }

    NotificationService *notifications() { return &m_notifications; }
    ToastService *toasts() { return &m_toasts; }
    NavigationService *navigation() { return m_navigation; }
    Translations *translations() { return &m_translations; }
    VehicleStore *vehicle() { return &m_vehicle; }
    int arrivalEvents() const { return m_arrivalEvents; }

    Q_INVOKABLE bool loadFixture(const QString &name)
    {
        QFile file(QStringLiteral(ROUTE_FIXTURE_DIR "/") + name + ".json");
        if (!file.open(QIODevice::ReadOnly))
            return false;
        m_route = RouteHelpers::parseRouteResponse(file.readAll());
        if (!m_route.isValid())
            return false;
        m_navigation->clearNavigation();
        publishPosition(m_route.waypoints.first());
        m_navigation->setRoute(m_route);
        return true;
    }

    Q_INVOKABLE void approach()
    {
        for (auto it = m_route.waypoints.crbegin(); it != m_route.waypoints.crend(); ++it) {
            if (it->distanceTo(m_route.waypoints.last()) > 60) {
                publishPosition(*it);
                return;
            }
        }
    }

    Q_INVOKABLE void calculate()
    {
        const auto destination = m_route.waypoints.last();
        m_navigation->setDestination(destination.latitude, destination.longitude);
    }

    Q_INVOKABLE void arrive() { publishPosition(m_route.waypoints.last()); }
    Q_INVOKABLE void depart() { publishPosition(m_route.waypoints.first()); }

    Q_INVOKABLE bool recalculate()
    {
        return QMetaObject::invokeMethod(m_navigation, "onRouteCalculated", Qt::DirectConnection,
                                         Q_ARG(Route, m_route));
    }

    Q_INVOKABLE void reconnect()
    {
        const auto destination = m_route.waypoints.last();
        m_repo.set("navigation", "latitude", QString::number(destination.latitude, 'f', 6));
        m_repo.set("navigation", "longitude", QString::number(destination.longitude, 'f', 6));
        m_repo.publish("navigation", "updated");
    }

    Q_INVOKABLE void removeMainBatteryParked()
    {
        m_repo.set("vehicle", "state", "parked");
        m_repo.set("aux-battery", "voltage", "11600");
        m_repo.set("battery:0", "present", "false");
    }

    Q_INVOKABLE void ride() { m_repo.set("vehicle", "state", "ready-to-drive"); }

    Q_INVOKABLE bool receive(const QString &json) { return m_ingress.receive(json); }

    Q_INVOKABLE void advance(int milliseconds)
    {
        m_now += milliseconds;
        QMetaObject::invokeMethod(&m_notifications, "expireEvents", Qt::DirectConnection);
    }

signals:
    void arrivalEventsChanged();

private:
    void publishPosition(const LatLng &position)
    {
        const QJsonObject sample{{"latitude", QString::number(position.latitude, 'f', 6)},
            {"longitude", QString::number(position.longitude, 'f', 6)}, {"state", "fix-established"},
            {"speed", "0"}, {"eph", "3"},
            {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        m_repo.publish("gps:tpv", QString::fromUtf8(QJsonDocument(sample).toJson(QJsonDocument::Compact)));
    }

    qint64 m_now = 0;
    int m_arrivalEvents = 0;
    QTcpServer m_router;
    InMemoryMdbRepository m_repo;
    GpsStore m_gps;
    NavigationStore m_navStore;
    VehicleStore m_vehicle;
    SettingsStore m_settings;
    SpeedLimitStore m_speedLimit;
    BatteryStore m_battery0;
    BatteryStore m_battery1;
    AuxBatteryStore m_auxBattery;
    CbBatteryStore m_cbBattery;
    BackupBatteryMonitor *m_backupMonitor;
    NotificationService m_notifications;
    NotificationIngress m_ingress;
    ToastService m_toasts;
    Translations m_translations;
    NavigationService *m_navigation;
    Route m_route;
};

class NotificationQuickTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void applicationAvailable()
    {
        qmlRegisterType<NativeAttentionHarness>("ScootUITest", 1, 0, "NativeAttentionHarness");
        for (const auto *font : {"Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Medium.ttf",
                                 "MaterialIcons-Regular.otf"})
            QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/") + font);
        QGuiApplication::setFont(QFont(QStringLiteral("Roboto")));
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        const QString directory = qEnvironmentVariable("SCOOTUI_TEST_CAPTURE_DIR");
        if (!directory.isEmpty())
            QDir().mkpath(directory);
        engine->rootContext()->setContextProperty(QStringLiteral("captureDirectory"), directory);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(notification, NotificationQuickTestSetup)
#include "NotificationQuickTest.moc"
