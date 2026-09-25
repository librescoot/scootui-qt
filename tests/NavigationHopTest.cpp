#include <QtTest>
#include <QDateTime>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "routing/ValhallaClient.h"
#include "routing/BlockedRoadSelector.h"
#include "services/AddressDatabaseService.h"
#include "services/NavigationService.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"

const QString AddressDatabaseService::MbtilesPath =
    QStringLiteral("/nonexistent/navigation-hop-test.mbtiles");

class NavigationHopTest : public QObject
{
    Q_OBJECT
private slots:
    void twoImmediateAppendsRetainBothStops();
    void clearWhilePlanLoadingCannotEraseExternalRoute();
    void concurrentExternalAppendRetainsBothStops();
    void staleProgressDoesNotAdvanceReplacement();
    void queuedArrivalCannotReachReplacement();
    void reachedStopAdvancesAndFinalArrivalRetainsOwner();
    void moveJumpAndRemovePreserveTarget();
    void rebootRestoresOwnerSnapshot();
    void rebootCompletedPlanDoesNotRearmArrival();
    void clearDoesNotRestoreLegacySettings();
    void appendAfterFinalArrivalReopensPrompt();
    void unavailableOwnerReportsErrorWithoutBlocking();
    void keepStopIsDurableAndClearedOnDismount();
    void blockedRoadSelectionTracksProgress();
    void blockedRoadIsSentOnlyOnGuidanceRequests();
    void blockedRoadCanBeClearedAndDoesNotSurviveHopChange();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        GpsStore gps{&repo};
        SpeedLimitStore speed{&repo};
        NavigationStore navStore{&repo};
        VehicleStore vehicle{&repo};
        SettingsStore settings{&repo};
        NavigationService nav{&gps, &navStore, &vehicle, &settings, &speed, &repo};
        Fixture() {
            gps.start(); navStore.start(); vehicle.start(); settings.start(); speed.start();
            repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
                     QStringLiteral("ready-to-drive"));
        }
        ~Fixture() {
            if (auto *client = nav.findChild<ValhallaClient *>()) client->cancelPending();
        }
    };
    static void external(InMemoryMdbRepository &repo, const QString &method,
                         const QJsonObject &payload) {
        const QJsonObject envelope{{QStringLiteral("reply_channel"), QStringLiteral("test:reply")},
                                   {QStringLiteral("method"), method},
                                   {QStringLiteral("payload"), payload}};
        repo.push(QStringLiteral("settings:route-plan"),
                  QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    }
    static QJsonObject stop(double lat, double lon, const QString &label) {
        return {{QStringLiteral("lat"), lat}, {QStringLiteral("lon"), lon},
                {QStringLiteral("label"), label}};
    }
    static QJsonObject snapshot(InMemoryMdbRepository &repo) {
        return QJsonDocument::fromJson(repo.get(QStringLiteral("navigation"),
                                                 QStringLiteral("plan")).toUtf8()).object();
    }
    static void gps(Fixture &f, double lat, double lon) {
        f.repo.publish(QStringLiteral("gps:tpv"), QStringLiteral(
            "{\"latitude\":\"%1\",\"longitude\":\"%2\",\"course\":\"90\","
            "\"speed\":\"20\",\"eph\":\"4.5\",\"state\":\"fix-established\","
            "\"timestamp\":\"%3\"}")
            .arg(lat, 0, 'f', 7).arg(lon, 0, 'f', 7)
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));
    }
};

void NavigationHopTest::blockedRoadSelectionTracksProgress()
{
    Route route;
    route.waypoints = {{52.5, 13.4}, {52.5, 13.401}, {52.5, 13.402}};
    const LatLng start{52.5, 13.4};
    const LatLng ahead = BlockedRoadSelector::ahead(route, start, 0);
    QVERIFY(ahead.isValid());
    QVERIFY(ahead.distanceTo(start) > 55.0);
    QVERIFY(ahead.distanceTo(start) < 65.0);
    QVERIFY(!BlockedRoadSelector::ahead(route, {52.5, 13.402}, 1).isValid());
    QVERIFY(!BlockedRoadSelector::ahead(route, {52.501, 13.4}, 0).isValid());
    QVERIFY(!BlockedRoadSelector::ahead(route, start, -1).isValid());
}

void NavigationHopTest::blockedRoadIsSentOnlyOnGuidanceRequests()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QByteArray routeRequest;
    QByteArray previewRequest;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
            const QByteArray request = socket->readAll();
            if (request.startsWith("GET /status")) {
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
            } else if (request.startsWith("POST /route")) {
                if (request.contains("exclude_locations"))
                    routeRequest = request;
                else
                    previewRequest = request;
                socket->write("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
            }
            socket->flush();
            socket->disconnectFromHost();
        });
    });
    ValhallaClient client;
    client.setEndpoint(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort()));
    QTRY_VERIFY(client.isHealthy());
    client.setBlockedLocation({52.5, 13.401});
    RouteOrigin origin;
    origin.position = {52.5, 13.4};
    client.requestRoute(origin, {52.5, 13.402}, ValhallaClient::Reason::RoadBlocked);
    QTRY_VERIFY(!routeRequest.isEmpty());
    QVERIFY(routeRequest.contains("13.401"));
    client.requestPreviewRoute(origin, QList<LatLng>{{52.5, 13.402}});
    QTRY_VERIFY(!previewRequest.isEmpty());
    QVERIFY(!previewRequest.contains("exclude_locations"));
}

void NavigationHopTest::blockedRoadCanBeClearedAndDoesNotSurviveHopChange()
{
    Fixture f;
    gps(f, 52.5, 13.4);
    Route route;
    route.waypoints = {{52.5, 13.4}, {52.5, 13.401}, {52.5, 13.402}};
    RouteInstruction arrival;
    arrival.type = ManeuverType::Arrive;
    arrival.originalShapeIndex = 2;
    route.instructions = {arrival};
    route.distance = 140;
    route.duration = 30;
    f.nav.setRoute(route);
    QVERIFY(f.nav.canAvoidRoad());
    f.nav.avoidRoadAhead();
    QVERIFY(f.nav.blockedRoadActive());
    QVERIFY(!f.nav.canAvoidRoad());
    f.nav.clearRoadAvoidance();
    QVERIFY(!f.nav.blockedRoadActive());
    f.nav.avoidRoadAhead();
    QVERIFY(f.nav.blockedRoadActive());
    f.nav.setRoutePlan(QVariantList{QVariantMap{{QStringLiteral("lat"), 52.51},
                                                {QStringLiteral("lon"), 13.41}}});
    QTRY_VERIFY_WITH_TIMEOUT(!f.nav.blockedRoadActive(), 3000);
}

void NavigationHopTest::twoImmediateAppendsRetainBothStops()
{
    Fixture f;
    f.nav.appendStop(52.51, 13.41, QStringLiteral("A"));
    f.nav.appendStop(52.52, 13.42, QStringLiteral("B"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 2, 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 2);
    QCOMPARE(f.nav.planStops().first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("A"));
    QVERIFY(f.repo.getAll(QStringLiteral("settings")).isEmpty());
}

void NavigationHopTest::clearWhilePlanLoadingCannotEraseExternalRoute()
{
    Fixture f;
    f.nav.appendStop(52.51, 13.41, QStringLiteral("queued"));
    f.nav.clearNavigation();
    QVERIFY(f.nav.errorMessage().contains(QStringLiteral("loading")));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 1);
}

void NavigationHopTest::concurrentExternalAppendRetainsBothStops()
{
    Fixture f;
    f.nav.appendStop(52.51, 13.41, QStringLiteral("A"));
    external(f.repo, QStringLiteral("plan.append"),
             {{QStringLiteral("stop"), stop(52.52, 13.42, QStringLiteral("B"))}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 2, 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 2);
}

void NavigationHopTest::staleProgressDoesNotAdvanceReplacement()
{
    Fixture f;
    gps(f, 52.50, 13.40);
    f.nav.setRoutePlan(QVariantList{QVariantMap{{QStringLiteral("lat"), 52.51},
        {QStringLiteral("lon"), 13.41}}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 3000);
    const QString oldId = snapshot(f.repo).value(QStringLiteral("id")).toString();
    const QString oldStopId = snapshot(f.repo).value(QStringLiteral("stops")).toArray()
        .first().toObject().value(QStringLiteral("id")).toString();
    external(f.repo, QStringLiteral("plan.replace"),
             {{QStringLiteral("stops"), QJsonArray{stop(52.6, 13.5, QStringLiteral("new"))}}});
    external(f.repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), oldId},
              {QStringLiteral("expected_stop_id"), oldStopId}});
    external(f.repo, QStringLiteral("plan.advance"),
             {{QStringLiteral("expected_plan_id"), oldId},
              {QStringLiteral("expected_stop_id"), oldStopId}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.destLatitude(), 52.6, 3000);
    QCOMPARE(f.nav.destAddress(), QStringLiteral("new"));
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 1);
}

void NavigationHopTest::queuedArrivalCannotReachReplacement()
{
    Fixture f;
    gps(f, 52.50, 13.40);
    f.nav.appendStop(52.51, 13.41, QStringLiteral("old"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 3000);
    Route route;
    route.waypoints = {{52.50, 13.40}, {52.51, 13.41}};
    RouteInstruction arrival;
    arrival.type = ManeuverType::Arrive;
    arrival.originalShapeIndex = 1;
    route.instructions = {arrival};
    route.distance = 100;
    route.duration = 60;
    f.nav.setRoute(route);
    gps(f, 52.51, 13.41); // queues a guarded plan.reached
    external(f.repo, QStringLiteral("plan.replace"),
             {{QStringLiteral("stops"), QJsonArray{stop(52.6, 13.5, QStringLiteral("new"))}}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.destAddress(), QStringLiteral("new"), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(f.nav.errorMessage().contains(QStringLiteral("stale")), 3000);
    const QJsonObject plan = snapshot(f.repo);
    QVERIFY(!plan.value(QStringLiteral("stops")).toArray().first().toObject()
                 .value(QStringLiteral("reached")).toBool());
    QCOMPARE(f.nav.planState(), int(RoutePlanState::Navigating));
}

void NavigationHopTest::reachedStopAdvancesAndFinalArrivalRetainsOwner()
{
    Fixture f;
    gps(f, 52.50, 13.40);
    f.nav.setRoutePlan(QVariantList{
        QVariantMap{{QStringLiteral("lat"), 52.51}, {QStringLiteral("lon"), 13.41}},
        QVariantMap{{QStringLiteral("lat"), 52.52}, {QStringLiteral("lon"), 13.42}}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 2, 3000);
    // Owner progress from another client triggers the arrival prompt.
    const QJsonObject plan = snapshot(f.repo);
    external(f.repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), plan.value(QStringLiteral("id"))},
              {QStringLiteral("expected_stop_id"), plan.value(QStringLiteral("stops"))
                   .toArray().first().toObject().value(QStringLiteral("id"))}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.planState(), int(RoutePlanState::AtStop), 3000);
    f.nav.confirmContinue();
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 1, 3000);
    QCOMPARE(f.nav.destLatitude(), 52.52);
    const QJsonObject next = snapshot(f.repo);
    external(f.repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), next.value(QStringLiteral("id"))},
              {QStringLiteral("expected_stop_id"), next.value(QStringLiteral("stops"))
                   .toArray().at(1).toObject().value(QStringLiteral("id"))}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.planState(), int(RoutePlanState::Complete), 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 2);
    QCOMPARE(f.nav.status(), int(NavigationStatus::Arrived));
}

void NavigationHopTest::moveJumpAndRemovePreserveTarget()
{
    Fixture f;
    f.nav.setRoutePlan(QVariantList{
        QVariantMap{{QStringLiteral("lat"), 52.51}, {QStringLiteral("lon"), 13.41}},
        QVariantMap{{QStringLiteral("lat"), 52.52}, {QStringLiteral("lon"), 13.42}},
        QVariantMap{{QStringLiteral("lat"), 52.53}, {QStringLiteral("lon"), 13.43}}}, 1);
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 1, 3000);
    QCOMPARE(f.nav.destLatitude(), 52.52);
    f.nav.moveStop(2, 0);
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 2, 3000);
    QCOMPARE(f.nav.destLatitude(), 52.52);
    f.nav.jumpToStop(0);
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 0, 3000);
    QCOMPARE(f.nav.destLatitude(), 52.53);
    f.nav.removeStop(1);
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 2, 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 2);
}

void NavigationHopTest::rebootRestoresOwnerSnapshot()
{
    InMemoryMdbRepository repo;
    external(repo, QStringLiteral("plan.replace"),
             {{QStringLiteral("stops"), QJsonArray{stop(52.51, 13.41, QStringLiteral("A")),
                                                    stop(52.52, 13.42, QStringLiteral("B"))}},
              {QStringLiteral("start_step"), 1}});
    GpsStore gps(&repo); SpeedLimitStore speed(&repo); NavigationStore store(&repo);
    VehicleStore vehicle(&repo); SettingsStore settings(&repo);
    NavigationService nav(&gps, &store, &vehicle, &settings, &speed, &repo);
    gps.start(); store.start(); vehicle.start(); settings.start(); speed.start();
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"), QStringLiteral("ready-to-drive"));
    QTRY_COMPARE_WITH_TIMEOUT(nav.currentStep(), 1, 3000);
    QCOMPARE(nav.stopCount(), 2);
    QCOMPARE(nav.destLatitude(), 52.52);
    if (auto *client = nav.findChild<ValhallaClient *>()) client->cancelPending();
}

void NavigationHopTest::rebootCompletedPlanDoesNotRearmArrival()
{
    InMemoryMdbRepository repo;
    external(repo, QStringLiteral("plan.append"),
             {{QStringLiteral("stop"), stop(52.51, 13.41, QStringLiteral("A"))}});
    const QJsonObject current = snapshot(repo);
    external(repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), current.value(QStringLiteral("id"))},
              {QStringLiteral("expected_stop_id"), current.value(QStringLiteral("stops"))
                   .toArray().first().toObject().value(QStringLiteral("id"))}});
    GpsStore gpsStore(&repo); SpeedLimitStore speed(&repo); NavigationStore store(&repo);
    VehicleStore vehicle(&repo); SettingsStore settings(&repo);
    NavigationService nav(&gpsStore, &store, &vehicle, &settings, &speed, &repo);
    gpsStore.start(); store.start(); vehicle.start(); settings.start(); speed.start();
    QSignalSpy arrived(&nav, &NavigationService::arrived);
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"));
    QTRY_COMPARE_WITH_TIMEOUT(nav.planState(), int(RoutePlanState::Complete), 3000);
    QCOMPARE(arrived.count(), 0);
    QVERIFY(!nav.hasRoute());
    QCOMPARE(snapshot(repo).value(QStringLiteral("stops")).toArray().size(), 1);
    nav.appendStop(52.52, 13.42, QStringLiteral("B"));
    QTRY_COMPARE_WITH_TIMEOUT(nav.planState(), int(RoutePlanState::AtStop), 3000);
    QCOMPARE(nav.stopCount(), 2);
}

void NavigationHopTest::appendAfterFinalArrivalReopensPrompt()
{
    Fixture f;
    f.nav.appendStop(52.51, 13.41, QStringLiteral("A"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 3000);
    const QJsonObject plan = snapshot(f.repo);
    external(f.repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), plan.value(QStringLiteral("id"))},
              {QStringLiteral("expected_stop_id"), plan.value(QStringLiteral("stops"))
                   .toArray().first().toObject().value(QStringLiteral("id"))}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.planState(), int(RoutePlanState::Complete), 3000);
    f.nav.appendStop(52.52, 13.42, QStringLiteral("B"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.planState(), int(RoutePlanState::AtStop), 3000);
    QCOMPARE(f.nav.stopCount(), 2);
    f.nav.confirmContinue();
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 1, 3000);
}

void NavigationHopTest::keepStopIsDurableAndClearedOnDismount()
{
    Fixture f;
    f.nav.appendStop(52.51, 13.41, QStringLiteral("A"));
    f.nav.appendStop(52.52, 13.42, QStringLiteral("B"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 2, 3000);
    const QJsonObject plan = snapshot(f.repo);
    external(f.repo, QStringLiteral("plan.reached"),
             {{QStringLiteral("expected_plan_id"), plan.value(QStringLiteral("id"))},
              {QStringLiteral("expected_stop_id"), plan.value(QStringLiteral("stops"))
                   .toArray().first().toObject().value(QStringLiteral("id"))}});
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.planState(), int(RoutePlanState::AtStop), 3000);
    f.nav.keepCurrentStop();
    QTRY_VERIFY_WITH_TIMEOUT(snapshot(f.repo).value(QStringLiteral("keep_current_stop")).toBool(), 3000);
    QCOMPARE(f.nav.planState(), int(RoutePlanState::Navigating));
    f.repo.set(QStringLiteral("vehicle"), QStringLiteral("state"), QStringLiteral("stand-by"));
    QTRY_VERIFY_WITH_TIMEOUT(!snapshot(f.repo).value(QStringLiteral("keep_current_stop")).toBool(), 3000);
}

void NavigationHopTest::unavailableOwnerReportsErrorWithoutBlocking()
{
    class UnavailableRepo : public InMemoryMdbRepository {
    public:
        void push(const QString &channel, const QString &command) override {
            if (channel != QLatin1String("settings:route-plan"))
                InMemoryMdbRepository::push(channel, command);
        }
    } repo;
    GpsStore gps(&repo); SpeedLimitStore speed(&repo); NavigationStore store(&repo);
    VehicleStore vehicle(&repo); SettingsStore settings(&repo);
    NavigationService nav(&gps, &store, &vehicle, &settings, &speed, &repo);
    gps.start(); store.start(); vehicle.start(); settings.start(); speed.start();
    QElapsedTimer timer;
    timer.start();
    nav.appendStop(52.51, 13.41);
    QVERIFY(timer.elapsed() < 100);
    QTRY_COMPARE_WITH_TIMEOUT(nav.status(), int(NavigationStatus::Error), 7500);
    QVERIFY(nav.errorMessage().contains(QStringLiteral("timed out")));
    QVERIFY(!nav.hasPlan());
    QVERIFY(repo.get(QStringLiteral("navigation"), QStringLiteral("plan")).isEmpty());
}

void NavigationHopTest::clearDoesNotRestoreLegacySettings()
{
    Fixture f;
    f.repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.route-plan.0.latitude"),
               QStringLiteral("52.51"));
    f.nav.appendStop(52.52, 13.42);
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 3000);
    f.nav.clearNavigation();
    QTRY_VERIFY_WITH_TIMEOUT(!f.nav.hasPlan(), 3000);
    QCOMPARE(snapshot(f.repo).value(QStringLiteral("stops")).toArray().size(), 0);
}

QTEST_MAIN(NavigationHopTest)
#include "NavigationHopTest.moc"
