#include <QtTest>

#include "routing/RouteModels.h"

// RoutePlan is pure data plus a few index helpers. These tests pin the
// invariants the hop state machine relies on: the step never escapes the
// stop list, atLastStop() is false for an empty plan, and stop lookups by
// index or id degrade to safe defaults instead of reading out of bounds.
class RoutePlanTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyPlanIsInvalidAndNotLast();
    void singleStopIsBothCurrentAndLast();
    void clampStepKeepsIndexInRange();
    void currentAndNextStopFollowTheStep();
    void stopAndIdLookupsAreSafe();
    void hopPreviewLookupByDestination();
};

static RouteStop makeStop(int id, double lat, double lon, const QString &label = {})
{
    RouteStop s;
    s.id = id;
    s.position = {lat, lon};
    s.label = label;
    return s;
}

void RoutePlanTest::emptyPlanIsInvalidAndNotLast()
{
    RoutePlan plan;
    QVERIFY(!plan.isValid());
    QCOMPARE(plan.stopCount(), 0);
    QVERIFY(!plan.atLastStop());
    QVERIFY(!plan.currentStop().isValid());
    QVERIFY(!plan.nextStop().isValid());
    QCOMPARE(plan.indexOfStopId(0), -1);
    QCOMPARE(plan.nextStopId(), -1);
}

void RoutePlanTest::singleStopIsBothCurrentAndLast()
{
    RoutePlan plan;
    plan.stops = {makeStop(7, 52.5, 13.4, QStringLiteral("Only"))};
    QVERIFY(plan.isValid());
    QCOMPARE(plan.stopCount(), 1);
    QVERIFY(plan.atLastStop());
    QCOMPARE(plan.currentStop().id, 7);
    QVERIFY(!plan.nextStop().isValid());
    QCOMPARE(plan.nextStopId(), -1);
}

void RoutePlanTest::clampStepKeepsIndexInRange()
{
    RoutePlan plan;
    plan.stops = {makeStop(1, 52.5, 13.4), makeStop(2, 52.6, 13.5), makeStop(3, 52.7, 13.6)};

    plan.currentStep = 5;
    plan.clampStep();
    QCOMPARE(plan.currentStep, 2);

    plan.currentStep = -3;
    plan.clampStep();
    QCOMPARE(plan.currentStep, 0);

    plan.stops.clear();
    plan.currentStep = 4;
    plan.clampStep();
    QCOMPARE(plan.currentStep, 0);
}

void RoutePlanTest::currentAndNextStopFollowTheStep()
{
    RoutePlan plan;
    plan.stops = {makeStop(1, 52.5, 13.4, QStringLiteral("A")),
                  makeStop(2, 52.6, 13.5, QStringLiteral("B")),
                  makeStop(3, 52.7, 13.6, QStringLiteral("C"))};
    plan.currentStep = 1;

    QCOMPARE(plan.currentStop().id, 2);
    QCOMPARE(plan.currentStop().label, QStringLiteral("B"));
    QCOMPARE(plan.nextStop().id, 3);
    QVERIFY(!plan.atLastStop());

    plan.currentStep = 2;
    QVERIFY(plan.atLastStop());
    QVERIFY(!plan.nextStop().isValid());
}

void RoutePlanTest::stopAndIdLookupsAreSafe()
{
    RoutePlan plan;
    plan.stops = {makeStop(10, 52.5, 13.4), makeStop(20, 52.6, 13.5)};

    QCOMPARE(plan.stopAt(0).id, 10);
    QCOMPARE(plan.stopAt(1).id, 20);
    QVERIFY(!plan.stopAt(-1).isValid());
    QVERIFY(!plan.stopAt(2).isValid());

    QCOMPARE(plan.indexOfStopId(20), 1);
    QCOMPARE(plan.indexOfStopId(999), -1);
}

void RoutePlanTest::hopPreviewLookupByDestination()
{
    RoutePlan plan;
    plan.stops = {makeStop(1, 52.5, 13.4), makeStop(2, 52.6, 13.5), makeStop(3, 52.7, 13.6)};

    HopPreview first;
    first.fromStopId = 1;
    first.toStopId = 2;
    first.distance = 1200;
    first.ready = true;
    HopPreview second;
    second.fromStopId = 2;
    second.toStopId = 3;
    plan.hops = {first, second};

    QVERIFY(plan.hopTo(2) != nullptr);
    QCOMPARE(plan.hopTo(2)->distance, 1200.0);
    QVERIFY(plan.hopTo(3) != nullptr);
    QCOMPARE(plan.hopTo(3)->fromStopId, 2);
    QVERIFY(plan.hopTo(1) == nullptr);
    QVERIFY(plan.hopTo(-1) == nullptr);
}

QTEST_MAIN(RoutePlanTest)
#include "RoutePlanTest.moc"
