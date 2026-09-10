#include <QtTest>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <memory>
#include <atomic>
#include <QScopeGuard>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include "services/StreetQueryDispatcher.h"

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
#include "stores/ThemeStore.h"

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
    void roundaboutPrefetchBeforeActivation_data() {
        QTest::addColumn<bool>("finishBeforeActivation");
        QTest::newRow("completed-prefetch") << true;
        QTest::newRow("in-flight-prefetch") << false;
    }
    void roundaboutPrefetchBeforeActivation() {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        QSKIP("Production roundabout Shapes require Qt 6.7+");
#else
        QFETCH(bool, finishBeforeActivation);
        Fixture f;
        auto matchGate = std::make_shared<Gate>();
        prepare(f, matchGate);
        delete f.road.m_streets;
        auto gate = std::make_shared<Gate>();
        auto calls = std::make_shared<std::atomic<int>>(0);
        f.road.m_streets = new StreetQueryDispatcher(&f.road, [gate, calls](const auto &) {
            ++*calls;
            gate->entered.release();
            gate->release.acquire();
            return StreetQueryResult{{}, true};
        });
        const auto release = qScopeGuard([gate]() { gate->release.release(100); });
        connect(f.road.m_streets, &StreetQueryDispatcher::ready, &f.road, &RoadInfoService::streetsReady);
        f.gps.start();
        f.repo.publish("gps:tpv", R"({"latitude":"51.993","longitude":"13","state":"fix-established","timestamp":"2026-09-09T00:00:00Z"})");
        Route r;
        r.waypoints = {{51.993, 13}, {51.9999, 13}, {52, 13.000162},
                       {52.0001, 13}, {52.0003, 13}};
        RouteInstruction enter, exit;
        enter.type = ManeuverType::RoundaboutEnter; enter.originalShapeIndex = 1;
        enter.location = r.waypoints[1];
        exit.type = ManeuverType::RoundaboutExit; exit.originalShapeIndex = 3;
        exit.location = r.waypoints[3];
        r.instructions = {enter, exit};
        r.distance = 900;
        f.nav.setRoute(r);
        QVERIFY(f.nav.currentManeuverDistance() > 500);
        const auto geometry = f.nav.currentRoundaboutRender();
        QVERIFY(geometry.value("ringValid").toBool());
        QTRY_COMPARE(calls->load(), 1); // no QML/TBT/icon exists yet
        matchGate->release.release();
        f.road.m_matcher->submit({});
        QTRY_VERIFY(!f.road.m_matcher->running()); // icon I/O cannot occupy matcher worker
        if (finishBeforeActivation) {
            gate->release.release();
            QTRY_VERIFY(!f.road.m_streets->running());
            QVERIFY(f.road.cachedStreets(geometry)["complete"].toBool());
        }
        ThemeStore theme(&f.settings);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("roadInfoService", &f.road);
        engine.rootContext()->setContextProperty("navigationService", &f.nav);
        engine.rootContext()->setContextProperty("themeStore", &theme);
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(SCOOTUI_SOURCE_DIR "/qml/widgets/navigation/TurnByTurnWidget.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        QQuickItem host(window.contentItem());
        window.show();
        std::unique_ptr<QObject> widget(component.create());
        QVERIFY2(widget, qPrintable(component.errorString()));
        qobject_cast<QQuickItem *>(widget.get())->setParentItem(&host);
        const auto findIcon = [&]() -> QObject * {
            for (auto *child : widget->findChildren<QObject *>())
                if (child->metaObject()->indexOfProperty("streetRequest") >= 0) return child;
            return nullptr;
        };
        QVERIFY(!findIcon()); // actual TBT Loader is still outside its threshold
        f.repo.publish("gps:tpv", R"({"latitude":"51.998","longitude":"13","state":"fix-established","timestamp":"2026-09-09T00:00:01Z"})");
        QVERIFY(f.nav.currentManeuverDistance() < 500);
        QTRY_VERIFY(findIcon());
        QPointer<QObject> icon = findIcon();
        QVERIFY(icon->property("hasMap").toBool());
        QCOMPARE(calls->load(), 1);
        QVERIFY(!f.road.m_streets->hasPending());
        if (!finishBeforeActivation) gate->release.release();
        QTRY_VERIFY(icon->property("streetsComplete").toBool());
        QCOMPARE(calls->load(), 1);
        host.setVisible(false); // warmed hidden screen cancels demand
        QTRY_VERIFY(icon.isNull());
        host.setVisible(true);
        QTRY_VERIFY(findIcon());
        icon = findIcon();
        QVERIFY(icon->property("streetsComplete").toBool());
        QCOMPARE(calls->load(), 1);

        // Same-index reroute must rebuild the actual navigation render snapshot.
        for (auto &point : r.waypoints) point.longitude += 0.001;
        f.nav.setRoute(r);
        QVERIFY(f.nav.currentRoundaboutRender() != geometry);
        QVERIFY(!f.road.cachedStreets(geometry)["complete"].toBool());
        QTRY_COMPARE(calls->load(), 2);
        // Reload while this prefetch/demand is blocked rejects its old-map reply.
        const auto path = f.road.m_dbPath;
        f.road.closeDb();
        QVERIFY(f.road.openDb(path));
        QCoreApplication::processEvents();
        gate->release.release();
        QTRY_COMPARE(calls->load(), 3);
        QVERIFY(!icon->property("streetsComplete").toBool());
        gate->release.release();
        QTRY_VERIFY(icon->property("streetsComplete").toBool());
        f.nav.clearNavigation();
        QCoreApplication::processEvents();
        QTRY_VERIFY(icon.isNull());
        QVERIFY(!f.road.m_streets->hasPending());
#endif
    }

    void roundaboutMissingTileKeepsCurrentGeometry_data() {
        QTest::addColumn<bool>("unusableFirst");
        QTest::newRow("retain-valid-partial") << false;
        QTest::newRow("recover-after-unusable-partial") << true;
    }

    void roundaboutMissingTileKeepsCurrentGeometry() {
        QFETCH(bool, unusableFirst);
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        QSKIP("Production roundabout Shapes require Qt 6.7+");
#else
        Fixture f;
        QVariantList path;
        for (const auto &p : {QVariantList{51.9995, 13.0}, QVariantList{51.9999, 13.0},
                              QVariantList{52.0, 13.000162}, QVariantList{52.0001, 13.0},
                              QVariantList{52.0003, 13.0}})
            path.append(QVariant(p));
        const QVariantList ring{path[1], path[2], path[3]};
        const QVariantMap data{{"centerLat", 52.0}, {"centerLon", 13.0}, {"ringRadius", 12},
                              {"ringValid", false}, {"entryIndex", 1}, {"exitIndex", 3}, {"path", path}};
        delete f.road.m_streets;
        auto calls = std::make_shared<std::atomic<int>>(0);
        f.road.m_streets = new StreetQueryDispatcher(&f.road, [calls, ring, unusableFirst](const auto &) {
            const int call = ++*calls;
            if (unusableFirst && call == 1)
                return StreetQueryResult{{QVariantMap{{"roundabout", false}, {"points", ring}}}, false};
            const int stage = call - int(unusableFirst);
            if (stage == 2) return StreetQueryResult{{}, false};
            return StreetQueryResult{{QVariantMap{{"roundabout", true}, {"points", ring}}}, stage >= 3};
        });
        connect(f.road.m_streets, &StreetQueryDispatcher::ready, &f.road, &RoadInfoService::streetsReady);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("roadInfoService", &f.road);
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(SCOOTUI_SOURCE_DIR "/qml/widgets/navigation/RoundaboutIconFromMap.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> icon(component.createWithInitialProperties({{"renderData", data}}));
        QVERIFY(icon);
        QTRY_COMPARE(icon->property("streetRequest").toDouble(), 0.0);
        if (unusableFirst) {
            QVERIFY(!icon->property("hasMap").toBool());
            const QVariant rawStreets = icon->property("streets");
            const QVariant streets = rawStreets.metaType() == QMetaType::fromType<QJSValue>()
                ? rawStreets.value<QJSValue>().toVariant() : rawStreets;
            QCOMPARE(streets.toList().size(), 1);
            QVERIFY(QMetaObject::invokeMethod(icon.get(), "requestStreets"));
            QTRY_COMPARE(icon->property("streetRequest").toDouble(), 0.0);
            QCOMPARE(calls->load(), 2);
        }
        QVERIFY(icon->property("hasMap").toBool());
        QVERIFY(!icon->property("streetsComplete").toBool());
        const auto layout = icon->property("layout").value<QJSValue>().toVariant();
        // Deterministically invoke the same function as the missing-tile retry timer.
        QVERIFY(QMetaObject::invokeMethod(icon.get(), "requestStreets"));
        QTRY_COMPARE(icon->property("streetRequest").toDouble(), 0.0);
        QCOMPARE(calls->load(), 2 + int(unusableFirst));
        QVERIFY(icon->property("hasMap").toBool());
        QCOMPARE(icon->property("layout").value<QJSValue>().toVariant(), layout);
        QVERIFY(QMetaObject::invokeMethod(icon.get(), "requestStreets"));
        QTRY_VERIFY(icon->property("streetsComplete").toBool());
        QCOMPARE(calls->load(), 3 + int(unusableFirst));
        QVERIFY(icon->property("hasMap").toBool());
#endif
    }

    void roundaboutQmlServiceSeam() {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        QSKIP("Production roundabout Shapes require Qt 6.7+");
#else
        Fixture f;
        delete f.road.m_streets;
        auto gate = std::make_shared<Gate>();
        auto calls = std::make_shared<std::atomic<int>>(0);
        f.road.m_streets = new StreetQueryDispatcher(&f.road, [gate, calls](const auto &) {
            ++*calls;
            gate->entered.release();
            gate->release.acquire();
            return StreetQueryResult{{QVariantMap{{"name", "current"}, {"points", QVariantList{}}}}, true};
        });
        const auto release = qScopeGuard([gate]() { gate->release.release(100); });
        connect(f.road.m_streets, &StreetQueryDispatcher::ready, &f.road, &RoadInfoService::streetsReady);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("roadInfoService", &f.road);
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(SCOOTUI_SOURCE_DIR "/qml/widgets/navigation/RoundaboutIconFromMap.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> icon(component.create());
        QVERIFY2(icon, qPrintable(component.errorString()));
        QVariantList path;
        for (const auto &p : {QVariantList{51.9995, 13.0}, QVariantList{51.9999, 13.0},
                              QVariantList{52.0, 13.00016}, QVariantList{52.0, 13.0005}})
            path.append(QVariant(p));
        QVariantMap data{{"centerLat", 52.0}, {"centerLon", 13.0}, {"ringRadius", 12},
                         {"ringValid", true}, {"entryIndex", 1}, {"exitIndex", 2}, {"path", path}};
        icon->setProperty("renderData", data);
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QVERIFY(icon->property("hasMap").toBool()); // route-only geometry during I/O
        const double first = icon->property("streetRequest").toDouble();
        icon->setProperty("size", 100);
        QCOMPARE(icon->property("streetRequest").toDouble(), first);
        QCOMPARE(calls->load(), 1);
        data["centerLon"] = 13.00001;
        icon->setProperty("renderData", data);
        const double latest = icon->property("streetRequest").toDouble();
        QVERIFY(latest != first);
        f.road.streetsReady(icon.get(), quint64(first), {QVariantMap{{"name", "stale"}}}, true);
        QCOMPARE(icon->property("streetRequest").toDouble(), latest);
        gate->release.release();
        QTRY_COMPARE(calls->load(), 2);
        QVERIFY(icon->property("hasMap").toBool());
        QVERIFY(!icon->property("streetsComplete").toBool());
        gate->release.release();
        QTRY_VERIFY(icon->property("streetsComplete").toBool());
        QVERIFY(icon->property("hasMap").toBool());
        const auto currentLayout = icon->property("layout").value<QJSValue>().toVariant();
        // Duplicate data and size changes do not fetch again or lose map features.
        icon->setProperty("renderData", data);
        QCOMPARE(calls->load(), 2);
        QCOMPARE(icon->property("layout").value<QJSValue>().toVariant(), currentLayout);

        // Real route signal invalidates service requests and refreshes QML bindings.
        emit f.nav.routeChanged();
        QTRY_COMPARE(calls->load(), 3);
        QVERIFY(icon->property("hasMap").toBool());
        // A map reload rejects that in-flight reply, even with identical route data.
        f.road.closeDb();
        QCoreApplication::processEvents();
        gate->release.release();
        QTRY_COMPARE(calls->load(), 4);
        QVERIFY(!icon->property("streetsComplete").toBool());
        gate->release.release();
        QTRY_VERIFY(icon->property("streetsComplete").toBool());

        // New unfit turn cannot keep the preceding ring while waiting for tiles.
        data["ringValid"] = false;
        icon->setProperty("renderData", data);
        QTRY_COMPARE(calls->load(), 5);
        QVERIFY(!icon->property("hasMap").toBool());
        icon.reset();
        QVERIFY(!f.road.m_streets->hasPending());
        gate->release.release();
        QTRY_VERIFY(!f.road.m_streets->running());
#endif
    }

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

QTEST_MAIN(RoadInfoServiceTest)
#include "RoadInfoServiceTest.moc"
