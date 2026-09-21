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
#include "core/ShortcutMenuItems.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/NavigationAvailabilityService.h"
#include "services/NotificationService.h"
#include "services/SettingsService.h"
#include "stores/BatteryStore.h"
#include "stores/EngineStore.h"
#include "stores/MenuStore.h"
#include "stores/SavedLocationsStore.h"
#include "stores/ScreenStore.h"
#include "stores/SettingsStore.h"
#include "stores/ShortcutMenuStore.h"
#define private public
#include "stores/TripStore.h"
#undef private

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
        for (const auto *font : {"Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Medium.ttf", "MaterialSymbolsOutlined-Filled.ttf"})
            QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral(":/ScootUI/assets/fonts/subset/") + font) >= 0);
        QFont::insertSubstitution(QStringLiteral("Material Icons"), QStringLiteral("Material Symbols Outlined"));
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
        // Preloaded components must go before the engine that compiled them.
        if (m_application)
            m_application->releaseQmlComponents();
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

    void tripCounterMenuUsesPersistentCapabilityAndCancelFirst()
    {
        auto *repo = m_application->m_repository.get();
        auto *menu = context<MenuStore>("menuStore");
        auto *trip = context<TripStore>("tripStore");
        auto *settings = context<SettingsStore>("settingsStore");
        QVERIFY(menu && trip && settings);

        const auto hasItem = [menu](const QString &id) {
            for (const QVariant &item : menu->currentItems()) {
                if (item.toMap().value("id").toString() == id)
                    return true;
            }
            return false;
        };
        const auto select = [menu](const QString &id) {
            int target = -1;
            const auto items = menu->currentItems();
            for (int i = 0; i < items.size(); ++i) {
                if (items.at(i).toMap().value("id").toString() == id) {
                    target = i;
                    break;
                }
            }
            QVERIFY2(target >= 0, qPrintable(id));
            while (menu->selectedIndex() != target)
                menu->navigateDown();
            menu->selectItem();
        };

        menu->open();
        QTest::qWait(160);
        select(QStringLiteral("settings"));
        select(QStringLiteral("settings_vehicle"));
        QVERIFY(!hasItem(QStringLiteral("settings_trip_counter")));
        menu->goBack();
        menu->goBack();

        repo->set("trip:counter", "api-version", "1");
        repo->set("settings", "trip.expunge", "age:365d");
        QVERIFY(trip->persistentAvailable());
        select(QStringLiteral("settings"));
        select(QStringLiteral("settings_vehicle"));
        select(QStringLiteral("settings_trip_counter"));
        QVERIFY(hasItem(QStringLiteral("trip_counter_reset_policy")));
        for (const QString &expected : {QStringLiteral("day"), QStringLiteral("battery"),
                                       QStringLiteral("manual"), QStringLiteral("ride")}) {
            select(QStringLiteral("trip_counter_reset_policy"));
            QCOMPARE(settings->tripCounterReset(), expected);
        }
        select(QStringLiteral("trip_counter_history"));
        QVERIFY(hasItem(QStringLiteral("trip_history_retention")));
        QVERIFY(hasItem(QStringLiteral("trip_history_value")));
        select(QStringLiteral("trip_history_value"));
        QCOMPARE(settings->tripExpunge(), QStringLiteral("age:730d"));
        select(QStringLiteral("trip_history_retention"));
        QCOMPARE(settings->tripExpunge(), QStringLiteral("count:500"));
        select(QStringLiteral("trip_history_retention"));
        QCOMPARE(settings->tripExpunge(), QStringLiteral("size:524288000"));
        select(QStringLiteral("trip_history_retention"));
        QCOMPARE(settings->tripExpunge(), QStringLiteral("never"));
        QVERIFY(!hasItem(QStringLiteral("trip_history_value")));
        menu->goBack();
        select(QStringLiteral("trip_counter_reset"));
        const auto confirmation = menu->currentItems();
        QCOMPARE(confirmation.size(), 2);
        QCOMPARE(confirmation.at(0).toMap().value("id").toString(),
                 QStringLiteral("trip_counter_reset_cancel"));
        QCOMPARE(confirmation.at(1).toMap().value("id").toString(),
                 QStringLiteral("trip_counter_reset_confirm"));
        QVERIFY(confirmation.at(1).toMap().value("caution").toBool());
    }

    void tripExpungeFallsBackAndPreservesCustomValue()
    {
        auto *repo = m_application->m_repository.get();
        auto *settings = context<SettingsStore>("settingsStore");
        auto *service = context<SettingsService>("settingsService");
        QVERIFY(settings && service);

        QVERIFY(!settings->tripExpungeAvailable());
        repo->set("settings", "trip.expunge", "age:14d");
        QVERIFY(settings->tripExpungeAvailable());
        QCOMPARE(settings->tripExpunge(), QStringLiteral("age:14d"));
        service->updateTripExpunge(QStringLiteral("age"));
        QCOMPARE(repo->get("settings", "trip.expunge"), QStringLiteral("age:14d"));
        service->updateTripExpunge(QStringLiteral("count"));
        QCOMPARE(repo->get("settings", "trip.expunge"), QStringLiteral("count:500"));
        service->updateTripExpunge(QStringLiteral("count"), QStringLiteral("1000"));
        QCOMPARE(repo->get("settings", "trip.expunge"), QStringLiteral("count:1000"));
        service->updateTripExpunge(QStringLiteral("age"), QStringLiteral("not-valid"));
        QCOMPARE(repo->get("settings", "trip.expunge"), QStringLiteral("count:1000"));
        repo->set("settings", "trip.expunge", "bad:value");
        QCOMPARE(settings->tripExpunge(), QStringLiteral("age:365d"));
        repo->hdel("settings", "trip.expunge");
        QTRY_VERIFY(!settings->tripExpungeAvailable());
        QCOMPARE(settings->tripExpunge(), QStringLiteral("age:365d"));
    }

    void tripExpungeGrammarMatchesBoundaryCorpus()
    {
        const QStringList valid{
            QStringLiteral("never"),
            QStringLiteral("age:1ns"),
            QStringLiteral("age:1us"),
            QStringLiteral("age:1.5ms"),
            QStringLiteral("age:1h30m"),
            QStringLiteral("age:1d"),
            QStringLiteral("age:106751d"),
            QStringLiteral("count:0"),
            QStringLiteral("count:9223372036854775807"),
            QStringLiteral("size:0"),
            QStringLiteral("size:9223372036854775807"),
        };
        const QStringList invalid{
            QStringLiteral("age:0"),
            QStringLiteral("age:0ns"),
            QStringLiteral("age:0.5ns"),
            QStringLiteral("age:.5us"),
            QStringLiteral("age:1.s"),
            QStringLiteral("age:01s"),
            QString::fromUtf8("age:1µs"),
            QString::fromUtf8("age:1μs"),
            QStringLiteral("age:106752d"),
            QStringLiteral("age:2562047h47m16.854775808s"),
            QStringLiteral("count:01"),
            QStringLiteral("count:+1"),
            QStringLiteral("count:9223372036854775808"),
            QStringLiteral("size:-1"),
            QStringLiteral("size:9223372036854775808"),
            QStringLiteral(" age:1s"),
            QStringLiteral("age: 1s"),
            QStringLiteral("age:1s "),
            QStringLiteral("count: 1"),
            QStringLiteral("size:1\t"),
        };

        for (const QString &value : valid)
            QVERIFY2(SettingsStore::isValidTripExpunge(value), qPrintable(value));
        for (const QString &value : invalid)
            QVERIFY2(!SettingsStore::isValidTripExpunge(value), qPrintable(value));
    }

    void tripCounterAcknowledgedResetPublishesSuccessToast()
    {
        auto *repo = m_application->m_repository.get();
        auto *trip = context<TripStore>("tripStore");
        auto *notifications = context<NotificationService>("notificationService");
        QVERIFY(trip && notifications);
        repo->set("trip:counter", "api-version", "1");
        trip->reset();
        QCOMPARE(trip->resetState(), QStringLiteral("pending"));
        const QString requestId = trip->m_pendingResetId;
        QVERIFY(!requestId.isEmpty());
        repo->publish("trip:command-result", QStringLiteral(
            R"({"id":"%1","op":"counter.reset","status":"ok","error":""})").arg(requestId));
        QCOMPARE(trip->resetState(), QStringLiteral("success"));
        QTRY_COMPARE(notifications->presentation().value("main").toMap().value("title").toString(),
                     QStringLiteral("Trip counter reset"));
        QCOMPARE(notifications->presentation().value("main").toMap().value("kind").toString(),
                 QStringLiteral("success"));
    }

    void quickDestinationsTrackAssignmentsAndRevalidateConfirmation()
    {
        auto *repo = m_application->m_repository.get();
        auto *engine = context<EngineStore>("engineStore");
        auto *saved = context<SavedLocationsStore>("savedLocationsStore");
        auto *shortcuts = context<ShortcutMenuStore>("shortcutMenuStore");
        auto *screens = context<ScreenStore>("screenStore");
        auto *availability = context<NavigationAvailabilityService>("navAvailabilityService");
        auto *settingsService = context<SettingsService>("settingsService");
        auto *settingsStore = context<SettingsStore>("settingsStore");
        QVERIFY(engine && saved && shortcuts && screens && availability && settingsService
                && settingsStore);

        repo->set("vehicle", "state", "ready-to-drive");
        repo->set("vehicle", "kickstand", "down");
        repo->set("engine-ecu", "speed", "0");
        availability->setOverride(true, true);
        QCOMPARE(shortcuts->actionCount(), 2);

        const auto addLocation = [repo](int id, double lat, double lng, const QString &label,
                                        const QString &uuid) {
            const QString prefix = QStringLiteral("dashboard.saved-locations.%1.").arg(id);
            repo->set("settings", prefix + "latitude", QString::number(lat, 'f', 7));
            repo->set("settings", prefix + "longitude", QString::number(lng, 'f', 7));
            repo->set("settings", prefix + "label", label);
            repo->set("settings", prefix + "uuid", uuid);
        };
        // The first location enters through the record-field migration.
        addLocation(0, 52.5, 13.4, QString(),
                    QStringLiteral("3fa85f64-5717-4562-b3fc-2c963f66afa6"));
        repo->set("settings", "dashboard.saved-locations.0.quick-slot", "1");
        QTRY_COMPARE(shortcuts->actionCount(), 3);
        QCOMPARE(shortcuts->actions().at(2).toMap().value("label").toString(),
                 QStringLiteral("52.50000, 13.40000"));
        // The second enters through an explicit items configuration.
        addLocation(1, 52.6, 13.5, QStringLiteral("Second address"),
                    QStringLiteral("0d6c21f0-0000-4000-8000-000000000001"));
        QTRY_COMPARE(saved->count(), 2);
        const QString secondUuid = saved->locations().at(1).toMap().value("uuid").toString();
        QVERIFY(!secondUuid.isEmpty());
        settingsService->updateShortcutMenuItems(ShortcutMenuItems::setDestinationSlot(
            ShortcutMenuItems::parse(settingsStore->shortcutMenuItems()), secondUuid, 2));
        QTRY_COMPARE(shortcuts->actionCount(), 4);

        shortcuts->show();
        shortcuts->cycle();
        shortcuts->cycle();
        shortcuts->confirm();
        QVERIFY(shortcuts->confirming());
        repo->set("settings", "dashboard.saved-locations.0.label", "Changed address");
        QTRY_VERIFY(!shortcuts->visible());
        repo->publish("input-events", "seatbox:press");
        QVERIFY(repo->get("navigation", "address").isEmpty());

        screens->setScreen(static_cast<int>(ScootEnums::ScreenMode::Cluster));
        shortcuts->show();
        shortcuts->cycle();
        shortcuts->cycle();
        repo->publish("input-events", "seatbox:release");
        QVERIFY(shortcuts->confirming());
        repo->publish("input-events", "seatbox:press");
        QTRY_COMPARE(repo->get("navigation", "address"), QStringLiteral("Changed address"));
        QCOMPARE(screens->currentScreenMode(), ScootEnums::ScreenMode::Map);
        QVERIFY(!shortcuts->visible());

        repo->set("navigation", "address", "unchanged");
        repo->set("engine-ecu", "speed", "0");
        shortcuts->show();
        shortcuts->cycle();
        shortcuts->cycle();
        shortcuts->confirm();
        repo->set("engine-ecu", "speed", "5");
        QTRY_VERIFY(!shortcuts->visible());
        QCOMPARE(shortcuts->actionCount(), 2);
        repo->publish("input-events", "seatbox:press");
        QCOMPARE(repo->get("navigation", "address"), QStringLiteral("unchanged"));

        repo->set("engine-ecu", "speed", "0");
        QTRY_COMPARE(shortcuts->actionCount(), 4);
        saved->deleteLocation(1);
        QTRY_COMPARE(shortcuts->actionCount(), 3);
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
