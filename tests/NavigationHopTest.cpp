#include <QtTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QTimer>

#include "routing/RouteModels.h"
#include "routing/ValhallaClient.h"
#include "services/AddressDatabaseService.h"
#include "services/NavigationService.h"
#include "services/RoutePlanService.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"

// The map service is never instantiated here, but MapService.cpp is linked for
// NavigationService's other call sites. Keep its startup probe away from
// installed maps.
const QString AddressDatabaseService::MbtilesPath =
    QStringLiteral("/nonexistent/navigation-hop-test.mbtiles");

// Hop state machine and navigation-hash ingest. These exercise the real
// NavigationService, stores, and in-memory repository, with Valhalla requests
// cancelled so nothing touches the network; routes are injected with setRoute().
class NavigationHopTest : public QObject
{
    Q_OBJECT

private slots:
    void reachIntermediateStopPromptsAndConfirmAdvances();
    void declineHoldsThenResumeReasks();
    void skipAdvancesAndCompletesOnLastStop();
    void autoAdvanceFiresWhenPromptTimesOut();
    void parkPausesAndResumeAdvancesWhenReached();
    void parkMidHopResumesSameHop();
    void reorderAndDeleteKeepTheCurrentTarget();
    void jumpToStopRetargetsAndMarksReached();
    void appendAfterFinalArrivalReopensTheTrip();
    void restoreFromSettingsStartsNavigating();
    void restoreFromReachedStopAdvances();
    void externalPlanPushStartsNavigation();
    void ownWriteEchoDoesNotRestart();
    void externalSingleDestinationReplacesPlan();
    void externalStepChangeKeepsThePlan();
    void externalClearStopsNavigation();
    void reconnectAfterArrivalDoesNotRearm();
    void planOverviewMapsLegsToRemainingStops();

private:
    struct Fixture {
        InMemoryMdbRepository repo;
        GpsStore gps{&repo};
        SpeedLimitStore speed{&repo};
        NavigationStore navStore{&repo};
        VehicleStore vehicle{&repo};
        SettingsStore settings{&repo};
        NavigationService nav{&gps, &navStore, &vehicle, &settings, &speed, &repo};

        Fixture()
        {
            gps.start();
            navStore.start();
            vehicle.start();
            settings.start();
            speed.start();
        }
    };

    // Drop any route request the service queued so it never reaches the network.
    static void quiesce(NavigationService &nav)
    {
        if (auto *client = nav.findChild<ValhallaClient *>())
            client->cancelPending();
    }

    static void quiesce(Fixture &f)
    {
        quiesce(f.nav);
    }

    static void setGps(Fixture &f, double lat, double lon)
    {
        const QString json = QStringLiteral(
            "{\"latitude\":\"%1\",\"longitude\":\"%2\",\"course\":\"90\",\"speed\":\"20\","
            "\"eph\":\"4.5\",\"state\":\"fix-established\",\"timestamp\":\"%3\"}")
            .arg(lat, 0, 'f', 7).arg(lon, 0, 'f', 7)
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        f.repo.publish(QStringLiteral("gps:tpv"), json);
    }

    static void setVehicleState(Fixture &f, const QString &state)
    {
        f.repo.set(QStringLiteral("vehicle"), QStringLiteral("state"), state);
    }

    static RouteStop stop(double lat, double lon, const QString &label)
    {
        RouteStop s;
        s.position = {lat, lon};
        s.label = label;
        return s;
    }

    static Route simpleRoute(const LatLng &from, const LatLng &to)
    {
        Route r;
        r.waypoints = {from, to};
        RouteInstruction start;
        start.type = ManeuverType::KeepStraight;
        start.isStart = true;
        start.originalShapeIndex = 0;
        start.location = from;
        start.distance = from.distanceTo(to);
        start.duration = 60;
        RouteInstruction arrive;
        arrive.type = ManeuverType::Arrive;
        arrive.originalShapeIndex = 1;
        arrive.location = to;
        r.instructions = {start, arrive};
        r.distance = from.distanceTo(to);
        r.duration = 60;
        return r;
    }

    static void startGuiding(Fixture &f, const QList<RouteStop> &stops)
    {
        setGps(f, 52.50, 13.40);
        f.nav.setRoutePlan(stops, 0);
        quiesce(f);
        f.nav.setRoute(simpleRoute({52.50, 13.40}, stops.first().position));
    }

    static void reachStop(Fixture &f, const LatLng &position)
    {
        setGps(f, position.latitude, position.longitude);
    }

    static QTimer *hopTimer(Fixture &f)
    {
        for (auto *timer : f.nav.findChildren<QTimer *>()) {
            if (timer->interval() == 1000)
                return timer;
        }
        return nullptr;
    }
};

void NavigationHopTest::reachIntermediateStopPromptsAndConfirmAdvances()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);

    QVERIFY(f.nav.hasPlan());
    QCOMPARE(f.nav.stopCount(), 2);
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Navigating));

    reachStop(f, stops[0].position);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::AtStop));
    QVERIFY(f.nav.hopPromptVisible());
    QCOMPARE(f.nav.hopPromptSecondsRemaining(), 25);
    QCOMPARE(f.nav.nextStopLabel(), QStringLiteral("B"));
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Arrived));
    QVERIFY(!f.nav.isNavigating());

    f.nav.confirmContinue();
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    QCOMPARE(f.nav.destLatitude(), stops[1].position.latitude);
    QCOMPARE(f.nav.destLongitude(), stops[1].position.longitude);
    QVERIFY(!f.nav.hopPromptVisible());
    quiesce(f);

    QSignalSpy arrivedSpy(&f.nav, &NavigationService::arrived);
    f.nav.setRoute(simpleRoute(stops[0].position, stops[1].position));
    reachStop(f, stops[1].position);

    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Complete));
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Arrived));
    QCOMPARE(arrivedSpy.count(), 1);
    QVERIFY(!f.nav.hopPromptVisible());

    // The persisted plan is dropped at the final stop.
    RoutePlanService persisted(&f.repo);
    QVERIFY(!persisted.load().isValid());
}

void NavigationHopTest::declineHoldsThenResumeReasks()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    reachStop(f, stops[0].position);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::AtStop));

    f.nav.declineContinue();
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Held));
    QVERIFY(!f.nav.hopPromptVisible());
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Idle));
    // Plan and step survive a hold.
    QVERIFY(f.nav.hasPlan());
    QCOMPARE(f.nav.currentStep(), 0);

    f.nav.resumePlan();
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::AtStop));
    QVERIFY(f.nav.hopPromptVisible());
    QCOMPARE(f.nav.currentStep(), 0);
}

void NavigationHopTest::skipAdvancesAndCompletesOnLastStop()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    reachStop(f, stops[0].position);

    f.nav.skipCurrentStop();
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    quiesce(f);

    // Skipping the last hop ends the trip.
    f.nav.skipCurrentStop();
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Complete));
    RoutePlanService persisted(&f.repo);
    QVERIFY(!persisted.load().isValid());
}

void NavigationHopTest::autoAdvanceFiresWhenPromptTimesOut()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    reachStop(f, stops[0].position);
    QCOMPARE(f.nav.hopPromptSecondsRemaining(), 25);

    QTimer *timer = hopTimer(f);
    QVERIFY(timer != nullptr);
    QVERIFY(timer->isActive());

    for (int i = 0; i < 25; ++i) {
        QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
    }
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    QVERIFY(!f.nav.hopPromptVisible());
}

void NavigationHopTest::parkPausesAndResumeAdvancesWhenReached()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    reachStop(f, stops[0].position);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::AtStop));

    setVehicleState(f, QStringLiteral("stand-by"));
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Paused));
    QVERIFY(f.nav.hasPlan());
    QCOMPARE(f.nav.currentStep(), 0);

    setVehicleState(f, QStringLiteral("ready-to-drive"));
    f.nav.resumePlan();
    // The stop was already reached, so resuming advances instead of returning.
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    quiesce(f);
}

void NavigationHopTest::parkMidHopResumesSameHop()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Navigating));

    setVehicleState(f, QStringLiteral("stand-by"));
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Paused));

    setVehicleState(f, QStringLiteral("ready-to-drive"));
    f.nav.resumePlan();
    // The stop was never reached, so the same hop is guided again.
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    quiesce(f);
}

void NavigationHopTest::reorderAndDeleteKeepTheCurrentTarget()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B")),
                                    stop(52.53, 13.43, QStringLiteral("C"))};
    startGuiding(f, stops);
    f.nav.skipCurrentStop(); // currentStep 1, target B
    quiesce(f);
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planStops().at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("B"));

    f.nav.removeStop(2); // after the current hop
    quiesce(f);
    QCOMPARE(f.nav.stopCount(), 2);
    QCOMPARE(f.nav.currentStep(), 1);

    f.nav.removeStop(0); // before the current hop
    quiesce(f);
    QCOMPARE(f.nav.stopCount(), 1);
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(f.nav.planStops().at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("B"));

    f.nav.appendStop(52.54, 13.44, QStringLiteral("D"));
    quiesce(f);
    QCOMPARE(f.nav.stopCount(), 2);
    QCOMPARE(f.nav.currentStep(), 0);

    f.nav.moveStop(1, 0); // D moves ahead of the current target B
    quiesce(f);
    QCOMPARE(f.nav.planStops().at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("D"));
    // The step follows B by id rather than staying on a fixed index.
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planStops().at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("B"));
}

void NavigationHopTest::externalPlanPushStartsNavigation()
{
    Fixture f;
    setGps(f, 52.50, 13.40);

    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    const QString json = RoutePlanService::serializeWaypoints(stops);
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"), json);
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("0"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.510000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.410000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("destination"),
               QStringLiteral("52.510000,13.410000"));

    QTRY_VERIFY_WITH_TIMEOUT(f.nav.hasPlan(), 2000);
    QCOMPARE(f.nav.stopCount(), 2);
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    quiesce(f);
}

void NavigationHopTest::ownWriteEchoDoesNotRestart()
{
    Fixture f;
    setGps(f, 52.50, 13.40);

    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"),
               RoutePlanService::serializeWaypoints(stops));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("0"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.510000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.410000"));
    QTRY_VERIFY_WITH_TIMEOUT(f.nav.hasPlan(), 2000);
    quiesce(f);

    QSignalSpy routeSpy(&f.nav, &NavigationService::routeChanged);
    // Same coordinate value, different string: the store sees a change, the
    // service must recognise it as its own echo and not restart the hop.
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.5100000"));
    QTest::qWait(250);

    QCOMPARE(routeSpy.count(), 0);
    QCOMPARE(f.nav.stopCount(), 2);
    QCOMPARE(f.nav.currentStep(), 0);
    quiesce(f);
}

// An external step change (CLI `lsc nav plan skip`, or a cloud writer moving
// current-step) carries the same waypoints with a different target. That target
// is not the current stop, which made the ingest mistake it for a new single
// destination and collapse the plan. Found on hardware.
void NavigationHopTest::externalStepChangeKeepsThePlan()
{
    Fixture f;
    setGps(f, 52.50, 13.40);

    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B")),
                                    stop(52.53, 13.43, QStringLiteral("C"))};
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"),
               RoutePlanService::serializeWaypoints(stops));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("0"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.510000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.410000"));
    QTRY_VERIFY_WITH_TIMEOUT(f.nav.hasPlan(), 2000);
    QCOMPARE(f.nav.stopCount(), 3);
    quiesce(f);

    // Same waypoints, step 1, target at stop B.
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("1"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.520000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.420000"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.currentStep(), 1, 2000);
    QCOMPARE(f.nav.stopCount(), 3);
    QCOMPARE(f.nav.destLatitude(), stops[1].position.latitude);
    QCOMPARE(f.nav.planStops().at(2).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("C"));
    quiesce(f);
}

void NavigationHopTest::externalSingleDestinationReplacesPlan()
{
    Fixture f;
    setGps(f, 52.50, 13.40);

    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"),
               RoutePlanService::serializeWaypoints(stops));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("0"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.510000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.410000"));
    QTRY_VERIFY_WITH_TIMEOUT(f.nav.hasPlan(), 2000);
    QCOMPARE(f.nav.stopCount(), 2);
    quiesce(f);

    // A legacy writer moves only the target, leaving the old waypoints behind.
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.600000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.500000"));
    QTRY_COMPARE_WITH_TIMEOUT(f.nav.stopCount(), 1, 2000);
    QCOMPARE(f.nav.currentStep(), 0);
    QCOMPARE(f.nav.destLatitude(), 52.6);
    quiesce(f);
}

void NavigationHopTest::externalClearStopsNavigation()
{
    Fixture f;
    setGps(f, 52.50, 13.40);

    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"),
               RoutePlanService::serializeWaypoints(stops));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QStringLiteral("0"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.510000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.410000"));
    QTRY_VERIFY_WITH_TIMEOUT(f.nav.hasPlan(), 2000);
    quiesce(f);

    f.repo.set(QStringLiteral("navigation"), QStringLiteral("waypoints"), QString());
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("current-step"), QString());
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QString());
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QString());
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("destination"), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!f.nav.hasPlan(), 2000);
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Idle));
}

// A store reconnect re-asserts the navigation hash. For a route injected with
// setRoute() (no plan), the re-asserted target matches the active destination
// and must be ignored; treating it as a new destination re-armed arrival and
// fired arrived() a second time.
void NavigationHopTest::reconnectAfterArrivalDoesNotRearm()
{
    Fixture f;
    setGps(f, 52.50, 13.40);
    quiesce(f);

    QSignalSpy arrivedSpy(&f.nav, &NavigationService::arrived);
    f.nav.setRoute(simpleRoute({52.50, 13.40}, {52.52, 13.42}));
    reachStop(f, {52.52, 13.42});
    QCOMPARE(arrivedSpy.count(), 1);
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Arrived));

    f.repo.set(QStringLiteral("navigation"), QStringLiteral("latitude"), QStringLiteral("52.520000"));
    f.repo.set(QStringLiteral("navigation"), QStringLiteral("longitude"), QStringLiteral("13.420000"));
    f.repo.publish(QStringLiteral("navigation"), QStringLiteral("updated"));
    QTest::qWait(250);

    QCOMPARE(arrivedSpy.count(), 1);
    QCOMPARE(f.nav.status(), static_cast<int>(NavigationStatus::Arrived));
    QVERIFY(!f.nav.hasPlan());
    quiesce(f);
}

// The overview is built from the per-leg preview the Valhalla client delivers.// Invoke the slot directly with synthetic legs to pin the mapping and totals,
// then confirm a step change re-bases the rows on the remaining stops.
void NavigationHopTest::jumpToStopRetargetsAndMarksReached()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B")),
                                    stop(52.53, 13.43, QStringLiteral("C"))};
    startGuiding(f, stops);

    f.nav.jumpToStop(2);
    quiesce(f);
    QCOMPARE(f.nav.currentStep(), 2);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    const QVariantList plan = f.nav.planStops();
    QVERIFY(plan.at(0).toMap().value(QStringLiteral("reached")).toBool());
    QVERIFY(plan.at(1).toMap().value(QStringLiteral("reached")).toBool());
    QVERIFY(!plan.at(2).toMap().value(QStringLiteral("reached")).toBool());
    QCOMPARE(f.nav.destLatitude(), stops[2].position.latitude);

    // Out-of-range jumps are ignored.
    f.nav.jumpToStop(-1);
    QCOMPARE(f.nav.currentStep(), 2);
    f.nav.jumpToStop(3);
    QCOMPARE(f.nav.currentStep(), 2);
    quiesce(f);
}

void NavigationHopTest::appendAfterFinalArrivalReopensTheTrip()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B"))};
    startGuiding(f, stops);
    reachStop(f, stops[0].position);
    f.nav.confirmContinue();
    quiesce(f);
    f.nav.setRoute(simpleRoute(stops[0].position, stops[1].position));
    reachStop(f, stops[1].position);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::Complete));

    // Adding a stop to a finished trip must re-open it at the reached stop and
    // offer the continue prompt, not replace the plan.
    f.nav.appendStop(52.53, 13.43, QStringLiteral("C"));
    quiesce(f);
    QCOMPARE(f.nav.stopCount(), 3);
    QCOMPARE(f.nav.currentStep(), 1);
    QCOMPARE(f.nav.planState(), static_cast<int>(RoutePlanState::AtStop));
    QVERIFY(f.nav.hopPromptVisible());
    QCOMPARE(f.nav.nextStopLabel(), QStringLiteral("C"));
    QVERIFY(f.nav.planStops().at(1).toMap().value(QStringLiteral("reached")).toBool());
}

// A restored trip navigates on its own instead of waiting for the rider to
// pick it from the menu again.
void NavigationHopTest::restoreFromSettingsStartsNavigating()
{
    InMemoryMdbRepository repo;

    RoutePlan plan;
    plan.stops = {stop(52.51, 13.41, QStringLiteral("A")),
                  stop(52.52, 13.42, QStringLiteral("B"))};
    RoutePlanService seeded(&repo);
    QVERIFY(seeded.save(plan, true));

    GpsStore gps(&repo);
    SpeedLimitStore speed(&repo);
    NavigationStore navStore(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    gps.start();
    navStore.start();
    vehicle.start();
    settings.start();
    speed.start();
    NavigationService nav(&gps, &navStore, &vehicle, &settings, &speed, &repo);

    QVERIFY(nav.hasPlan());
    QCOMPARE(nav.planState(), static_cast<int>(RoutePlanState::Navigating));
    QCOMPARE(nav.currentStep(), 0);
    quiesce(nav);
}

// A stop already marked reached means the rider was there, so the restored trip
// continues at the next stop; when that was the last one, the trip is done.
void NavigationHopTest::restoreFromReachedStopAdvances()
{
    InMemoryMdbRepository repo;

    RoutePlan plan;
    plan.stops = {stop(52.51, 13.41, QStringLiteral("A")),
                  stop(52.52, 13.42, QStringLiteral("B"))};
    plan.currentStep = 1;
    plan.stops[1].reached = true;
    RoutePlanService seeded(&repo);
    QVERIFY(seeded.save(plan, true));

    GpsStore gps(&repo);
    SpeedLimitStore speed(&repo);
    NavigationStore navStore(&repo);
    VehicleStore vehicle(&repo);
    SettingsStore settings(&repo);
    gps.start();
    navStore.start();
    vehicle.start();
    settings.start();
    speed.start();
    NavigationService nav(&gps, &navStore, &vehicle, &settings, &speed, &repo);

    QCOMPARE(nav.planState(), static_cast<int>(RoutePlanState::Complete));
    quiesce(nav);
}

void NavigationHopTest::planOverviewMapsLegsToRemainingStops()
{
    Fixture f;
    const QList<RouteStop> stops = {stop(52.51, 13.41, QStringLiteral("A")),
                                    stop(52.52, 13.42, QStringLiteral("B")),
                                    stop(52.53, 13.43, QStringLiteral("C"))};
    startGuiding(f, stops);

    QList<Route> legs;
    legs.append(simpleRoute({52.50, 13.40}, stops[0].position));
    legs.append(simpleRoute(stops[0].position, stops[1].position));
    legs.append(simpleRoute(stops[1].position, stops[2].position));
    QVERIFY(QMetaObject::invokeMethod(&f.nav, "onPlanPreviewReady", Qt::DirectConnection,
                                      Q_ARG(QList<Route>, legs)));

    const QVariantList overview = f.nav.planOverview();
    QCOMPARE(overview.size(), 3);
    QCOMPARE(overview[0].toMap().value(QStringLiteral("fromIndex")).toInt(), -1);
    QCOMPARE(overview[0].toMap().value(QStringLiteral("toIndex")).toInt(), 0);
    QCOMPARE(overview[0].toMap().value(QStringLiteral("toLabel")).toString(), QStringLiteral("A"));
    QCOMPARE(overview[1].toMap().value(QStringLiteral("fromIndex")).toInt(), 0);
    QCOMPARE(overview[1].toMap().value(QStringLiteral("toIndex")).toInt(), 1);
    QCOMPARE(overview[2].toMap().value(QStringLiteral("toLabel")).toString(), QStringLiteral("C"));
    QVERIFY(overview[0].toMap().value(QStringLiteral("ready")).toBool());
    QVERIFY(f.nav.planTotalDistance() > 0);
    QVERIFY(f.nav.planTotalDuration() > 0);

    // Advancing re-bases the overview on the stops that remain.
    f.nav.skipCurrentStop();
    quiesce(f);
    QList<Route> remainingLegs;
    remainingLegs.append(simpleRoute({52.51, 13.41}, stops[1].position));
    remainingLegs.append(simpleRoute(stops[1].position, stops[2].position));
    QVERIFY(QMetaObject::invokeMethod(&f.nav, "onPlanPreviewReady", Qt::DirectConnection,
                                      Q_ARG(QList<Route>, remainingLegs)));
    const QVariantList advanced = f.nav.planOverview();
    QCOMPARE(advanced.size(), 2);
    QCOMPARE(advanced[0].toMap().value(QStringLiteral("fromIndex")).toInt(), -1);
    QCOMPARE(advanced[0].toMap().value(QStringLiteral("toIndex")).toInt(), 1);
    QCOMPARE(advanced[1].toMap().value(QStringLiteral("toIndex")).toInt(), 2);

    f.nav.clearNavigation();
    QVERIFY(f.nav.planOverview().isEmpty());
    QCOMPARE(f.nav.planTotalDistance(), 0.0);
}

QTEST_MAIN(NavigationHopTest)
#include "NavigationHopTest.moc"
