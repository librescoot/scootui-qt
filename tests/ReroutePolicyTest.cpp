#include <QtTest>

#include "routing/ReroutePolicy.h"

class ReroutePolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void certainEstimateResistsParallelRoadGpsJump();
    void freshGpsUsesBoundedProjectionWhenEstimateUncertain();
    void staleGpsFallsBackOnlyToCertainEstimate();
    void presentationDepartureStartsAndCanRecover();
    void oneRequestPerDeviationEpisode();
    void coordinatesRequireFiniteGeographicRange();
};

void ReroutePolicyTest::certainEstimateResistsParallelRoadGpsJump()
{
    RerouteOriginSelector::Input input;
    input.gps = {52.5, 13.4, 90.0, 36.0, 20.0, 0.8,
                 QStringLiteral("2026-08-24T10:00:00Z"),
                 ScootEnums::GpsState::FixEstablished};
    input.gpsAgeMs = 700;
    input.physicalEstimate = {52.5001, 13.4};
    input.physicalUncertaintyMeters = 20.0;

    const RouteOrigin origin = RerouteOriginSelector::select(input);
    QCOMPARE(origin.position, input.physicalEstimate);
    QCOMPARE(origin.heading, 90.0);
    QCOMPARE(origin.headingToleranceDegrees, 40);
    QVERIFY(origin.radiusMeters >= 15);
    QVERIFY(origin.radiusMeters <= 50);
}

void ReroutePolicyTest::freshGpsUsesBoundedProjectionWhenEstimateUncertain()
{
    RerouteOriginSelector::Input input;
    input.gps = {52.5, 13.4, 90.0, 36.0, 6.0, 0.8,
                 QStringLiteral("2026-08-24T10:00:00Z"),
                 ScootEnums::GpsState::FixEstablished};
    input.gpsAgeMs = 700;
    input.physicalEstimate = {52.6, 13.5};
    input.physicalUncertaintyMeters = 80.0;

    const RouteOrigin origin = RerouteOriginSelector::select(input);
    QVERIFY(origin.isValid());
    const double projected = LatLng{52.5, 13.4}.distanceTo(origin.position);
    QVERIFY(projected > 9.0 && projected < 11.0);
}

void ReroutePolicyTest::staleGpsFallsBackOnlyToCertainEstimate()
{
    RerouteOriginSelector::Input input;
    input.gps = {52.5, 13.4, 90.0, 36.0, 5.0, 0.8,
                 QStringLiteral("2026-08-24T10:00:00Z"),
                 ScootEnums::GpsState::FixEstablished};
    input.gpsAgeMs = 5000;
    input.physicalEstimate = {52.51, 13.41};
    input.physicalUncertaintyMeters = 25.0;

    RouteOrigin origin = RerouteOriginSelector::select(input);
    QCOMPARE(origin.position, input.physicalEstimate);
    QVERIFY(origin.radiusMeters <= 50);

    input.physicalUncertaintyMeters = 80.0;
    QVERIFY(!RerouteOriginSelector::select(input).isValid());
}

void ReroutePolicyTest::presentationDepartureStartsAndCanRecover()
{
    QVERIFY(RouteDeparturePolicy::update(false, 20.0, true, 60.0, 35.0));
    // The departure notification is an edge, not a permanent latch: normal
    // distance hysteresis can clear off-route after physical reacquisition.
    QVERIFY(!RouteDeparturePolicy::update(true, 30.0, false, 60.0, 35.0));
    QVERIFY(!RouteDeparturePolicy::update(false, 30.0, false, 60.0, 35.0));
    QVERIFY(RouteDeparturePolicy::update(false, 61.0, false, 60.0, 35.0));
}

void ReroutePolicyTest::oneRequestPerDeviationEpisode()
{
    RerouteEpisodeGate gate;
    QVERIFY(!gate.shouldRequest(false, true));
    QVERIFY(gate.shouldRequest(true, true));
    QVERIFY(!gate.shouldRequest(true, true));
    QVERIFY(!gate.shouldRequest(true, false));

    gate.retryReady();
    QVERIFY(gate.shouldRequest(true, true));
    QVERIFY(!gate.shouldRequest(true, true));

    QVERIFY(!gate.shouldRequest(false, true));
    QVERIFY(gate.shouldRequest(true, true));
}

void ReroutePolicyTest::coordinatesRequireFiniteGeographicRange()
{
    QVERIFY((LatLng{52.0, 0.0}.isValid()));
    QVERIFY((LatLng{0.0, 13.0}.isValid()));
    QVERIFY(!LatLng{}.isValid());
    QVERIFY((!LatLng{91.0, 13.0}.isValid()));
    QVERIFY((!LatLng{52.0, 181.0}.isValid()));
    QVERIFY((!LatLng{std::nan(""), 13.0}.isValid()));
}

QTEST_APPLESS_MAIN(ReroutePolicyTest)
#include "ReroutePolicyTest.moc"
