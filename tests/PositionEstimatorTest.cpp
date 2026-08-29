#include <QtTest>

#include "services/PositionEstimator.h"

class PositionEstimatorTest : public QObject
{
    Q_OBJECT

private slots:
    void quantizedOdometerDoesNotBrakeBetweenEdges();
    void odometerResidualIsBoundedAfterEdge();
    void routeLockUsesPhysicalDistanceAndDwell();
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

void PositionEstimatorTest::routeLockUsesPhysicalDistanceAndDwell()
{
    RouteSnapState state;
    QVERIFY(state.locked());

    // The open-sky receiver normally reports about 19 m EPH. An ordinary
    // 20 m cross-track estimate must therefore remain visually snapped.
    QCOMPARE(RouteSnapState::breakAwayMeters(20.0), 30.0);
    QCOMPARE(RouteSnapState::relockMeters(20.0), 20.0);
    for (int i = 0; i < 100; ++i)
        QVERIFY(state.update(20.0, 100, 20.0));

    // A sustained position-only departure still releases after 1.5 seconds.
    for (int i = 0; i < 14; ++i)
        QVERIFY(state.update(31.0, 100, 20.0));
    QVERIFY(!state.update(31.0, 100, 20.0));

    // While the rider is clearly travelling across the expected route
    // direction, a smaller but real displacement releases after two seconds.
    state.reset(true);
    for (int i = 0; i < 19; ++i)
        QVERIFY(state.update(16.0, 100, 20.0, 50.0, true));
    QVERIFY(!state.update(16.0, 100, 20.0, 50.0, true));

    // Direction alone is not enough at the turn vertex: GPS course can lag the
    // route tangent there without the rider actually leaving the route.
    state.reset(true);
    for (int i = 0; i < 30; ++i)
        QVERIFY(state.update(10.0, 100, 20.0, 90.0, true));

    // Reacquisition requires both proximity and an aligned (or unavailable)
    // course, so continuing straight beside the requested turn cannot relock.
    state.reset(false);
    for (int i = 0; i < 30; ++i)
        QVERIFY(!state.update(15.0, 100, 20.0, 60.0, true));
    for (int i = 0; i < 19; ++i)
        QVERIFY(!state.update(15.0, 100, 20.0, 20.0, true));
    QVERIFY(state.update(15.0, 100, 20.0, 20.0, true));

    // Poor-but-accepted fixes get more room, but the cap still guarantees an
    // actual departure eventually becomes visible.
    QCOMPARE(RouteSnapState::breakAwayMeters(50.0), 45.0);
    QCOMPARE(RouteSnapState::relockMeters(50.0), 30.0);
}

QTEST_APPLESS_MAIN(PositionEstimatorTest)
#include "PositionEstimatorTest.moc"
