#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QNetworkProxy>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>

#define private public
#include "core/Application.h"
#undef private
#include "core/BootGate.h"
#include "core/EnvConfig.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/NotificationService.h"
#include "stores/BatteryStore.h"
#include "stores/ScreenStore.h"
#include "stores/SettingsStore.h"

QElapsedTimer g_bootTimer;

class ApplicationNotificationTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_home;
    std::unique_ptr<Application> m_application;
    std::unique_ptr<QQmlApplicationEngine> m_engine;

    template<typename T> T *context(const char *name) const
    {
        return qobject_cast<T *>(m_engine->rootContext()->contextProperty(QLatin1String(name)).value<QObject *>());
    }

    QString mainId() const
    {
        return context<NotificationService>("notificationService")->presentation()
            .value("main").toMap().value("id").toString();
    }

    void capture(QQuickWindow *window, const QString &name)
    {
        const QString path = qEnvironmentVariable("SCOOTUI_TEST_CAPTURE_DIR");
        if (path.isEmpty()) return;
        QDir().mkpath(path);
        QTest::qWait(150);
        const auto image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(path + "/" + name + ".png"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("SCOOTUI_REDIS_HOST", "none");
        qputenv("SCOOTUI_SIMULATOR", "0");
        qunsetenv("NOTIFY_SOCKET");
        qputenv("XDG_DATA_HOME", m_home.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", m_home.path().toUtf8());
        QStandardPaths::setTestModeEnabled(true);
        // Fail closed on loopback even if a background service attempts a request.
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 1));
        EnvConfig::initialize();
        g_bootTimer.start();
        for (const auto *font : {"Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Medium.ttf", "MaterialIcons-Regular.otf"})
            QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/") + font) >= 0);
        QGuiApplication::setFont(QFont("Roboto"));
    }

    void init()
    {
        m_application = std::make_unique<Application>();
        m_engine = std::make_unique<QQmlApplicationEngine>();
        QVERIFY(m_application->initialize(*m_engine));
        QVERIFY(m_application->isInMemoryBackend());
        QVERIFY(!m_application->isSimulatorMode());
        auto *repo = m_application->m_repository.get();
        QVERIFY(qobject_cast<InMemoryMdbRepository *>(repo));
        repo->set("settings", "dashboard.valhalla-url", "http://127.0.0.1:1");
        repo->set("vehicle", "state", "parked");
    }

    void cleanup()
    {
        m_engine.reset();
        m_application->shutdownBackgroundWorkers();
        m_application.reset();
    }

    void mapFirstHiddenClusterKeepsSurfaceOwnership()
    {
        auto *screens = context<ScreenStore>("screenStore");
        auto *notifications = context<NotificationService>("notificationService");
        auto *boot = context<BootGate>("bootGate");
        QVERIFY(screens && notifications && boot);
        m_application->m_repository->set("dashboard", "remote-screen", "Map");
        notifications->setCoverageWarning(true);
        m_engine->load(QUrl("qrc:/ScootUI/qml/Main.qml"));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(notifications->surface(), QString("map"));
        QTRY_COMPARE(mainId(), QString("map-coverage"));
        QVERIFY(!boot->clusterWarm());
        QVERIFY(!window->findChild<QObject *>("clusterAttention"));
        capture(window, "application-map-before-warm");

        // The actual first-frame gate requests this; avoid its platform-specific
        // boot-animation/systemctl handoff in an offscreen integration test.
        boot->requestWarmCluster();
        QTRY_VERIFY(boot->clusterWarm());
        auto *cluster = window->findChild<QQuickItem *>("clusterAttention");
        QVERIFY(cluster);
        QVERIFY(!cluster->isVisible());
        QCOMPARE(notifications->surface(), QString("map"));
        QCOMPARE(mainId(), QString("map-coverage"));
        capture(window, "application-map-after-hidden-warm");

        for (int i = 0; i < 2; ++i) {
            m_application->m_repository->set("dashboard", "remote-screen", "Cluster");
            QTRY_COMPARE(notifications->surface(), QString("cluster"));
            QTRY_VERIFY(mainId() != "map-coverage");
            QCOMPARE(window->findChild<QQuickItem *>("clusterAttention"), cluster);
            m_application->m_repository->set("dashboard", "remote-screen", "Map");
            QTRY_COMPARE(notifications->surface(), QString("map"));
            QTRY_COMPARE(mainId(), QString("map-coverage"));
            QVERIFY(!cluster->isVisible());
        }
        notifications->setCoverageWarning(false);
        QTRY_VERIFY(mainId() != "map-coverage");
        notifications->setCoverageWarning(true);
        QTRY_COMPARE(mainId(), QString("map-coverage"));
        // Maintenance is not a map surface, even when Map remains selected.
        m_application->m_repository->set("vehicle", "state", "stand-by");
        QTRY_COMPARE(notifications->surface(), QString("cluster"));
        QTRY_VERIFY(mainId() != "map-coverage");
        m_application->m_repository->set("vehicle", "state", "parked");
        QTRY_COMPARE(notifications->surface(), QString("map"));
        QTRY_COMPARE(mainId(), QString("map-coverage"));
    }

    void dualBatteryToggleRefreshesUnchangedSlotOneFault()
    {
        auto *repo = m_application->m_repository.get();
        auto *settings = context<SettingsStore>("settingsStore");
        auto *battery = context<BatteryStore>("battery1Store");
        QVERIFY(settings && battery);
        repo->set("settings", "scooter.dual-battery", "true");
        QVERIFY(settings->dualBattery());
        repo->addToSet("battery:1:fault", "6");
        repo->publish("battery:1", "fault");
        QTRY_VERIFY(!battery->faults().isEmpty());
        QTRY_COMPARE(mainId(), QString("battery-fault-1"));
        const auto faults = battery->faults();
        QSignalSpy faultChanges(battery, &BatteryStore::faultsChanged);
        for (int i = 0; i < 2; ++i) {
            repo->set("settings", "scooter.dual-battery", "false");
            QVERIFY(!settings->dualBattery());
            QTRY_VERIFY(mainId() != "battery-fault-1");
            QCOMPARE(battery->faults(), faults);
            repo->set("settings", "scooter.dual-battery", "true");
            QVERIFY(settings->dualBattery());
            QTRY_COMPARE(mainId(), QString("battery-fault-1"));
            QCOMPARE(battery->faults(), faults);
        }
        QCOMPARE(faultChanges.count(), 0);
    }
};

QTEST_MAIN(ApplicationNotificationTest)
#include "ApplicationNotificationTest.moc"
