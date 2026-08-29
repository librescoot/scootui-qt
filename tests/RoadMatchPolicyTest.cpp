#include <QtTest>

#include "services/RoadMatchPolicy.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/SpeedLimitStore.h"
#include "stores/SpeedLimitParser.h"

class RoadMatchPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void headingDisambiguatesCrossingRoads();
    void rejectsDistantRoads();
    void previousMatchAddsHysteresis();
    void parallelRoadNeedsClearWin();
    void transientMissesRetainRoadMatch();
    void freeDriveSnapUsesAcquireReleaseHysteresis();
    void convertsMphToKph();
    void freshLocalMatchBeatsRedisPoll();
};

void RoadMatchPolicyTest::headingDisambiguatesCrossingRoads()
{
    QList<RoadMatchCandidateScore> candidates{
        {QStringLiteral("north"), 2.0, 0.0, false},
        {QStringLiteral("east"), 1.0, 90.0, false},
    };
    auto selected = RoadMatchPolicy::select(candidates, 5.0, true, {}, 30.0);
    QCOMPARE(selected.index, 0);
    QVERIFY(selected.confident);
}

void RoadMatchPolicyTest::rejectsDistantRoads()
{
    QList<RoadMatchCandidateScore> candidates{
        {QStringLiteral("far"), 45.0, 0.0, false},
    };
    QCOMPARE(RoadMatchPolicy::select(candidates, 0.0, false, {}, 30.0).index,
             -1);
}

void RoadMatchPolicyTest::previousMatchAddsHysteresis()
{
    QList<RoadMatchCandidateScore> candidates{
        {QStringLiteral("old"), 5.0, 0.0, false},
        {QStringLiteral("new"), 3.0, 0.0, false},
    };
    auto selected = RoadMatchPolicy::select(
        candidates, 0.0, false, QStringLiteral("old"), 30.0);
    QCOMPARE(selected.index, 0);
    QVERIFY(selected.confident);
}

void RoadMatchPolicyTest::parallelRoadNeedsClearWin()
{
    QList<RoadMatchCandidateScore> candidates{
        {QStringLiteral("current"), 8.0, 0.0, false},
        {QStringLiteral("parallel"), 5.0, 2.0, false},
    };
    auto selected = RoadMatchPolicy::select(
        candidates, 0.0, true, QStringLiteral("current"), 30.0);
    QCOMPARE(selected.index, 0);

    // Once the alternative is more than the 4 m previous-match bonus better,
    // retaining the old carriageway would be the less plausible choice.
    candidates[1].distanceMeters = 3.0;
    selected = RoadMatchPolicy::select(
        candidates, 0.0, true, QStringLiteral("current"), 30.0);
    QCOMPARE(selected.index, 1);
}

void RoadMatchPolicyTest::transientMissesRetainRoadMatch()
{
    RoadMatchRetentionState retention;
    QVERIFY(retention.retainAfterMiss());
    QVERIFY(retention.retainAfterMiss());
    QVERIFY(!retention.retainAfterMiss());

    retention.matched();
    QCOMPARE(retention.consecutiveMisses(), 0);
    QVERIFY(retention.retainAfterMiss());
}

void RoadMatchPolicyTest::freeDriveSnapUsesAcquireReleaseHysteresis()
{
    FreeDriveSnapState state;
    QVERIFY(!state.update(true, 21.0));
    QVERIFY(state.update(true, 19.0));

    // Ordinary GPS drift does not pop an acquired marker off the road.
    QVERIFY(state.update(true, 29.0));
    QVERIFY(!state.update(true, 31.0));

    // Moving back inside the release band is insufficient; acquisition stays
    // conservative so a nearby courtyard is not presented as the street.
    QVERIFY(!state.update(true, 21.0));
    QVERIFY(state.update(true, 20.0));

    // A materially off-road or no-longer-confident match releases immediately.
    QVERIFY(!state.update(false, 5.0));
}

void RoadMatchPolicyTest::convertsMphToKph()
{
    QCOMPARE(SpeedLimitParser::resolve(QStringLiteral("30 mph")),
             QStringLiteral("48"));
    QCOMPARE(SpeedLimitParser::resolve(QStringLiteral("50 km/h")),
             QStringLiteral("50"));
    QCOMPARE(SpeedLimitParser::resolve(QStringLiteral("DE:zone30")),
             QStringLiteral("30"));
}

void RoadMatchPolicyTest::freshLocalMatchBeatsRedisPoll()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("speed-limit"), QStringLiteral("road-name"),
             QStringLiteral("stale"), false);
    repo.set(QStringLiteral("speed-limit"), QStringLiteral("speed-limit"),
             QStringLiteral("20"), false);
    SpeedLimitStore store(&repo);
    store.start();
    store.refreshAllFields();
    QCOMPARE(store.roadName(), QStringLiteral("stale"));

    store.setRoadNameDirect(QStringLiteral("matched"));
    store.setSpeedLimitDirect(QStringLiteral("30 mph"));
    repo.set(QStringLiteral("speed-limit"), QStringLiteral("road-name"),
             QStringLiteral("stale-again"));

    QCOMPARE(store.roadName(), QStringLiteral("matched"));
    QCOMPARE(store.speedLimit(), QStringLiteral("48"));
}

QTEST_APPLESS_MAIN(RoadMatchPolicyTest)
#include "RoadMatchPolicyTest.moc"
