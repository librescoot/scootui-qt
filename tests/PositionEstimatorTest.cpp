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

    for (int elapsed = 0; elapsed < 1400; elapsed += 100)
        QVERIFY(state.update(20.0, 100));
    QVERIFY(!state.update(20.0, 100));

    // Presentation snapping cannot reset this decision: only the supplied
    // physical distance is observed. A brief close pass is insufficient.
    for (int elapsed = 0; elapsed < 1900; elapsed += 100)
        QVERIFY(!state.update(3.0, 100));
    QVERIFY(state.update(3.0, 100));

    // One good sample interrupts a pending break-away dwell.
    for (int i = 0; i < 10; ++i)
        QVERIFY(state.update(20.0, 100));
    QVERIFY(state.update(2.0, 100));
    for (int i = 0; i < 10; ++i)
        QVERIFY(state.update(20.0, 100));
}

QTEST_APPLESS_MAIN(PositionEstimatorTest)
#include "PositionEstimatorTest.moc"
