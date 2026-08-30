#include <QtTest>

#include "services/PositionEstimator.h"

class PositionEstimatorTest : public QObject
{
    Q_OBJECT

private slots:
    void quantizedOdometerDoesNotBrakeBetweenEdges();
    void odometerResidualIsBoundedAfterEdge();
    void routePresentationFollowsNavigationState();
    void routePresentationStopsAtZeroSpeed();
};

void PositionEstimatorTest::quantizedOdometerDoesNotBrakeBetweenEdges()
{
    OdometerReconciler reconciler;
    double travelled = 0.0;
    constexpr double speedMs = 10.0;
    constexpr double dt = 0.05;

    // Seed at a non-zero 100 m bucket, then travel almost to the next edge
    // while the reported odometer remains unchanged.
    reconciler.advance(1000.0, 0.0, dt);
    for (int i = 0; i < 198; ++i)
        travelled += reconciler.advance(1000.0, speedMs, dt);

    QVERIFY(std::abs(travelled - 99.0) < 0.01);
}

void PositionEstimatorTest::odometerResidualIsBoundedAfterEdge()
{
    OdometerReconciler reconciler;
    reconciler.advance(1000.0, 0.0, 0.05);
    // First edge establishes a known bucket boundary; its partial interval is
    // deliberately not reconciled.
    for (int i = 0; i < 200; ++i)
        reconciler.advance(1000.0, 10.0, 0.05);
    reconciler.advance(1100.0, 10.0, 0.05);

    for (int i = 0; i < 180; ++i)
        reconciler.advance(1100.0, 9.0, 0.05); // estimator has 81 m

    const double edgeTick = reconciler.advance(1200.0, 9.0, 0.05);
    QVERIFY(edgeTick <= 9.0 * 0.05 * 1.20 + 1e-9);
    QVERIFY(edgeTick >= 9.0 * 0.05);
    QVERIFY(reconciler.pendingResidual() > 0.0);

    // Reconciliation never invents motion at a stop.
    QCOMPARE(reconciler.advance(1200.0, 0.0, 1.0), 0.0);
}

void PositionEstimatorTest::routePresentationFollowsNavigationState()
{
    QVERIFY(RoutePresentationPolicy::shouldSnapToRoute(true, false, false));
    QVERIFY(!RoutePresentationPolicy::shouldSnapToRoute(true, true, false));
    QVERIFY(!RoutePresentationPolicy::shouldSnapToRoute(true, false, true));
    QVERIFY(!RoutePresentationPolicy::shouldSnapToRoute(false, false, false));

    // Direction disagreement may steer the independent estimate, but cannot
    // itself release presentation or start a reroute.
    QVERIFY(RoutePresentationPolicy::hasDirectionalDepartureEvidence(
        16.0, 50.0, true));
    QVERIFY(!RoutePresentationPolicy::hasDirectionalDepartureEvidence(
        10.0, 90.0, true));
}

void PositionEstimatorTest::routePresentationStopsAtZeroSpeed()
{
    QVERIFY(!RoutePresentationPolicy::allowsPolylineWalk(0.0));
    QVERIFY(!RoutePresentationPolicy::allowsPolylineWalk(1.0));
    QVERIFY(RoutePresentationPolicy::allowsPolylineWalk(1.1));
    QVERIFY(!RoutePresentationPolicy::allowsPolylineWalk(std::nan("")));

}

QTEST_APPLESS_MAIN(PositionEstimatorTest)
#include "PositionEstimatorTest.moc"
