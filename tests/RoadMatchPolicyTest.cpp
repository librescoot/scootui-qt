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
