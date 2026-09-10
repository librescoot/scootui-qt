#include <QtTest>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <memory>

#include "services/RoadInfoService.h"
#include "services/RoadMatchDispatcher.h"
#include "services/RoadWorkerThreads.h"
#include "services/NavigationService.h"
#include "services/AddressDatabaseService.h"
#include "repositories/InMemoryMdbRepository.h"
#include "stores/GpsStore.h"
#include "stores/NavigationStore.h"
#include "stores/VehicleStore.h"
#include "stores/SettingsStore.h"
#include "stores/SpeedLimitStore.h"

// No address database service is instantiated by this focused integration test.
// Keep the production road service's startup probe away from installed maps.
const QString AddressDatabaseService::MbtilesPath = QStringLiteral("/nonexistent/road-info-test.mbtiles");

class RoadInfoServiceTest : public QObject
{
    Q_OBJECT

    struct Gate {
        QSemaphore entered;
        QSemaphore release;
        ~Gate() { release.release(100); }
    };

    struct Fixture {
        InMemoryMdbRepository repo;
        GpsStore gps{&repo};
        SpeedLimitStore speed{&repo};
        NavigationStore navStore{&repo};
        VehicleStore vehicle{&repo};
        SettingsStore settings{&repo};
        NavigationService nav{&gps, &navStore, &vehicle, &settings, &speed, &repo};
        RoadInfoService road{&gps, &speed, &nav};
        QTemporaryDir dir;
        Fixture() {
            // Real navigation mutation/signal paths, but no backend requests.
            for (auto *timer : nav.findChild<ValhallaClient *>()->findChildren<QTimer *>())
                timer->stop();
        }
    };

    static RoadMatchResult match() {
        RoadMatchResult r;
        r.selection = {0, true};
        r.chosen.name = QStringLiteral("Tile Street");
        r.chosen.refs = QStringLiteral("B 2");
        r.chosen.kind = QStringLiteral("primary");
        r.chosen.maxspeed = QStringLiteral("50");
        r.chosen.routeNetworks = QStringLiteral("DE:national");
        r.chosen.policy.key = QStringLiteral("tile-segment");
        r.chosen.policy.bearingDegrees = 90;
        r.chosen.lat1 = 52; r.chosen.lon1 = 13;
        r.chosen.lat2 = 52; r.chosen.lon2 = 13.001;
        r.chosen.actualDistanceMeters = 2;
        return r;
    }

    static Route route() {
        Route r;
        r.waypoints = {{52, 13}, {52, 13.001}};
        r.instructions.append(RouteInstruction{});
        return r;
    }

    void prepare(Fixture &f, const std::shared_ptr<Gate> &gate) {
        const QString path = f.dir.filePath(QStringLiteral("tiles.mbtiles"));
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("fixture"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB)")));
        }
        QSqlDatabase::removeDatabase(QStringLiteral("fixture"));
        QVERIFY(f.road.openDb(path));
        const int x = RoadInfoService::lonToTileX(13, 14);
        const int y = RoadInfoService::latToTileY(52, 14);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                f.road.insertTile((quint64(x + dx) << 32) | quint32(y + dy), {});
        delete f.road.m_matcher;
        f.road.m_matcher = new RoadMatchDispatcher(&f.road, [gate](const RoadMatchRequest &) {
            gate->entered.release();
            // Bounded fallback prevents a failed assertion from hanging cleanup.
            gate->release.tryAcquire(1, 5000);
            return match();
        });
        connect(f.road.m_matcher, &RoadMatchDispatcher::ready, &f.road, &RoadInfoService::applyMatch);
    }

    void seed(Fixture &f, const std::shared_ptr<Gate> &gate) {
        f.road.updateRoadInfo(52, 13);
        QVERIFY(gate->entered.tryAcquire(1, 1000));
        gate->release.release();
        QTRY_VERIFY(!f.road.m_matcher->running());
        QTRY_COMPARE(f.speed.speedLimit(), QStringLiteral("50"));
        QVERIFY(f.road.m_lastAcceptedMatchMs >= 0);
    }

    void verifyCleared(Fixture &f) {
        QVERIFY(!f.road.hasConfidentRoadMatch());
        QVERIFY(f.road.m_previousMatchKey.isEmpty());
        QCOMPARE(f.road.matchedSegmentLat1(), 0.0);
        QCOMPARE(f.road.matchedSegmentLon2(), 0.0);
        QCOMPARE(f.road.roadMatchDistanceMeters(), 0.0);
        QVERIFY(f.speed.speedLimit().isEmpty());
        QVERIFY(f.speed.roadName().isEmpty());
        QVERIFY(f.speed.roadRefs().isEmpty());
        QVERIFY(f.speed.roadType().isEmpty());
        QVERIFY(f.speed.roadSignStyle().isEmpty());
        QCOMPARE(f.speed.roadBearing(), -1.0);
    }

private slots:
    void cleanup() { RoadWorkerThreads::drainAfterEventLoop(); }

    void sustainedOverloadExpiresAcceptedOutput() {
        auto gate = std::make_shared<Gate>();
        Fixture f;
        prepare(f, gate);
        seed(f, gate);
        QVERIFY(f.road.hasConfidentRoadMatch());
        const qint64 accepted = f.road.m_lastAcceptedMatchMs;
        f.road.m_freshnessTimer.stop(); // Advance only the monotonic policy clock below.
        f.road.updateRoadInfo(52, 13);
        QVERIFY(gate->entered.tryAcquire(1, 1000));
        for (int second = 1; second <= 5; ++second) {
            // Every completion is superseded before it can reach publication.
            for (int burst = 0; burst < 10; ++burst)
                f.road.updateRoadInfo(52, 13 + second * 0.00001);
            QVERIFY(f.road.m_matcher->hasPending());
            gate->release.release();
            QTRY_COMPARE(f.road.m_matcher->staleCount(), quint64(second));
            QVERIFY(gate->entered.tryAcquire(1, 1000));
            f.road.checkTileFreshness(accepted + second * 1000);
            if (second < 3) {
                QCOMPARE(f.road.m_lastAcceptedMatchMs, accepted);
                QCOMPARE(f.speed.speedLimit(), QStringLiteral("50"));
            } else {
                verifyCleared(f);
                QCOMPARE(f.road.m_lastAcceptedMatchMs, qint64(-1));
            }
        }
        // The last running result is stale too; it must not resurrect output.
        f.road.m_matcher->invalidate();
        gate->release.release();
        QTRY_VERIFY(!f.road.m_matcher->running());
        QCOMPARE(f.road.m_matcher->staleCount(), quint64(6));
        verifyCleared(f);
    }

    void idleExpiryAndFreshAcceptedRefresh() {
        auto gate = std::make_shared<Gate>();
        Fixture f;
        prepare(f, gate);
        seed(f, gate);
        const auto accepted = f.road.m_lastAcceptedMatchMs;
        f.road.checkTileFreshness(accepted + RoadInfoService::TileFreshnessMs - 1);
        QVERIFY(f.road.hasConfidentRoadMatch());
        f.road.checkTileFreshness(accepted + RoadInfoService::TileFreshnessMs);
        verifyCleared(f);
        seed(f, gate);
        QVERIFY(f.road.hasConfidentRoadMatch());
        // Refresh an already retained result; the old deadline cannot clear it.
        f.road.m_lastAcceptedMatchMs = 0;
        QTest::qWait(2);
        seed(f, gate);
        QVERIFY(f.road.m_lastAcceptedMatchMs > 0);
        f.road.checkTileFreshness(RoadInfoService::TileFreshnessMs);
        QVERIFY(f.road.hasConfidentRoadMatch());
        f.road.checkTileFreshness(f.road.m_lastAcceptedMatchMs + RoadInfoService::TileFreshnessMs - 1);
        QCOMPARE(f.speed.roadName(), QStringLiteral("Tile Street"));

        // Actual timer wiring: no GPS or submissions after the accepted output.
        f.road.m_lastAcceptedMatchMs = f.road.m_freshnessClock.elapsed();
        QTRY_VERIFY_WITH_TIMEOUT(!f.road.hasConfidentRoadMatch(), 3500);
        verifyCleared(f);
    }

    void validityLossClearsGeometryAndRejectsBlockedResult_data() {
        QTest::addColumn<bool>("gpsLoss");
        QTest::newRow("gps-validity-loss") << true;
        QTest::newRow("position-provider-loss") << false;
    }

    void validityLossClearsGeometryAndRejectsBlockedResult() {
        QFETCH(bool, gpsLoss);
        auto gate = std::make_shared<Gate>();
        Fixture f;
        prepare(f, gate);
        seed(f, gate);
        QVERIFY(f.road.hasConfidentRoadMatch());
        f.road.updateRoadInfo(52, 13);
        QVERIFY(gate->entered.tryAcquire(1, 1000));
        f.road.updateRoadInfo(52, 13.00001);
        if (gpsLoss)
            emit f.gps.sampleChanged();
        else
            f.road.onVehiclePositionChanged(); // No map provider remains.
        QVERIFY(!f.road.m_hasLastPosition);
        QVERIFY(!f.road.m_matcher->hasPending());
        verifyCleared(f);
        gate->release.release();
        QTRY_VERIFY(!f.road.m_matcher->running());
        QCOMPARE(f.road.m_matcher->staleCount(), quint64(1));
        verifyCleared(f);
    }

    void routeMetadataSurvivesTileExpiryAndValidityLoss() {
        auto gate = std::make_shared<Gate>();
        Fixture f;
        prepare(f, gate);
        Route r = route();
        EdgeAttrs attrs;
        attrs.names = {QStringLiteral("Route Street"), QStringLiteral("B 9")};
        r.shapeAttrs = {attrs}; // Partial route: speed, class and bearing from tile.
        f.nav.setRoute(r);
        seed(f, gate);
        QCOMPARE(f.speed.roadName(), QStringLiteral("Route Street"));
        QCOMPARE(f.speed.roadRefs(), QStringLiteral("B 9"));
        f.road.checkTileFreshness(f.road.m_lastAcceptedMatchMs + RoadInfoService::TileFreshnessMs);
        QCOMPARE(f.speed.roadName(), QStringLiteral("Route Street"));
        QCOMPARE(f.speed.roadRefs(), QStringLiteral("B 9"));
        QVERIFY(f.speed.speedLimit().isEmpty());
        QVERIFY(f.speed.roadType().isEmpty());
        QCOMPARE(f.speed.roadBearing(), -1.0);
        seed(f, gate);
        f.road.updateRoadInfo(52, 13);
        QVERIFY(gate->entered.tryAcquire(1, 1000));
        // Actual GPS signal with the invalid default sample.
        emit f.gps.sampleChanged();
        QVERIFY(!f.road.m_hasLastPosition);
        QVERIFY(f.speed.speedLimit().isEmpty());
        QCOMPARE(f.speed.roadName(), QStringLiteral("Route Street"));
        gate->release.release();
        QTRY_VERIFY(!f.road.m_matcher->running());
        QVERIFY(f.speed.speedLimit().isEmpty());
    }

    void blockedMatchRejectsNavigationMutations_data() {
        QTest::addColumn<int>("mutation");
        QTest::newRow("route-replacement") << 0;
        QTest::newRow("route-clear") << 1;
        QTest::newRow("enrichment-created") << 2;
        QTest::newRow("enrichment-replaced") << 3;
        QTest::newRow("enrichment-removed") << 4;
    }

    void blockedMatchRejectsNavigationMutations() {
        QFETCH(int, mutation);
        auto gate = std::make_shared<Gate>();
        Fixture f;
        prepare(f, gate);
        Route r = route();
        if (mutation >= 3) {
            EdgeAttrs old;
            old.names = {QStringLiteral("Old Route")};
            r.shapeAttrs = {old};
        }
        f.nav.setRoute(r);
        f.road.updateRoadInfo(52, 13);
        QVERIFY(gate->entered.tryAcquire(1, 1000));
        f.road.updateRoadInfo(52, 13.00001);
        QVERIFY(f.road.m_matcher->hasPending());
        const auto generation = f.road.m_routeGeneration;
        EdgeAttrs attrs;
        attrs.names = {QStringLiteral("Current Route")};
        attrs.roadClass = QStringLiteral("secondary");
        attrs.speedLimitKph = 30;
        if (mutation == 0) {
            r.shapeAttrs = {attrs};
            f.nav.setRoute(r);
        } else if (mutation == 1) {
            f.nav.clearNavigation();
        } else {
            const QList<EdgeAttrs> values{mutation == 4 ? EdgeAttrs{} : attrs};
            QVERIFY(QMetaObject::invokeMethod(&f.nav, "onRouteAttributesReady", Qt::DirectConnection,
                                              Q_ARG(QList<EdgeAttrs>, values)));
        }
        QVERIFY(f.road.m_routeGeneration > generation);
        QVERIFY(!f.road.m_matcher->hasPending());
        // Keep this test focused on the invalidated active/pending jobs.
        f.road.m_rematchTimer.stop();
        gate->release.release();
        QTRY_VERIFY(!f.road.m_matcher->running());
        QCOMPARE(f.road.m_matcher->staleCount(), quint64(1));
        QVERIFY(!gate->entered.tryAcquire());
        QVERIFY(!f.road.hasConfidentRoadMatch());
        QCOMPARE(f.speed.roadBearing(), -1.0);
        if (mutation == 1 || mutation == 4) {
            QVERIFY(f.speed.roadName().isEmpty());
            QVERIFY(f.speed.speedLimit().isEmpty());
        } else {
            QCOMPARE(f.speed.roadName(), QStringLiteral("Current Route"));
            QCOMPARE(f.speed.speedLimit(), QStringLiteral("30"));
        }
    }
};

QTEST_GUILESS_MAIN(RoadInfoServiceTest)
#include "RoadInfoServiceTest.moc"
