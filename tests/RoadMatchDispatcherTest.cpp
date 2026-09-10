#include <QtTest>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QScopeGuard>
#include "services/RoadWorkerThreads.h"
#include <atomic>
#include <memory>
#include "services/RoadMatchDispatcher.h"

namespace {
struct Gate {
    QSemaphore entered;
    QSemaphore release;
    std::atomic<int> calls{0};
    std::atomic<QThread *> workerThread{nullptr};
    RoadMatchResult compute(const RoadMatchRequest &request)
    {
        workerThread.store(QThread::currentThread());
        ++calls;
        entered.release();
        release.acquire();
        RoadMatchResult result;
        result.chosen.name = QString::number(request.lat);
        return result;
    }
};
// Releases blocked work even if a test assertion returns early.
struct ReleaseOnExit {
    std::shared_ptr<Gate> gate;
    ~ReleaseOnExit() { gate->release.release(100); }
};
}

class RoadMatchDispatcherTest : public QObject
{
    Q_OBJECT
private slots:
    void latestWinsWithBoundedWorkAndGuiSeparation()
    {
        auto gate = std::make_shared<Gate>();
        RoadMatchDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QSignalSpy ready(&dispatcher, &RoadMatchDispatcher::ready);
        bool deliveredOnGui = false;
        connect(&dispatcher, &RoadMatchDispatcher::ready, this, [&]() {
            deliveredOnGui = QThread::currentThread() == thread();
        });
        RoadMatchRequest request;
        dispatcher.submit(request);
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QVERIFY(gate->workerThread.load() != QThread::currentThread());
        bool guiEventRan = false;
        QTimer::singleShot(0, this, [&]() { guiEventRan = true; });
        QTRY_VERIFY(guiEventRan);
        for (int i = 1; i <= 100; ++i) {
            request.lat = i;
            request.routeGeneration = 7;
            request.mapGeneration = 9;
            dispatcher.submit(request);
        }
        QCOMPARE(gate->calls.load(), 1);
        QVERIFY(dispatcher.running());
        QVERIFY(dispatcher.hasPending());
        QCOMPARE(dispatcher.replacedCount(), quint64(99));
        gate->release.release();
        QTRY_COMPARE(dispatcher.staleCount(), quint64(1));
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QCOMPARE(ready.count(), 0);
        QCOMPARE(gate->calls.load(), 2);
        gate->release.release();
        QTRY_COMPARE(ready.count(), 1);
        const auto accepted = qvariant_cast<RoadMatchRequest>(ready.at(0).at(0));
        QCOMPARE(accepted.lat, 100.0);
        QCOMPARE(accepted.sequence, dispatcher.latestSequence());
        QCOMPARE(accepted.routeGeneration, quint64(7));
        QCOMPARE(accepted.mapGeneration, 9);
        QVERIFY(deliveredOnGui);
        QVERIFY(!dispatcher.running());
        QVERIFY(!dispatcher.hasPending());
        QVERIFY(dispatcher.lastSnapshotAgeMs() >= 0);
    }

    void invalidationDropsActiveAndPending()
    {
        auto gate = std::make_shared<Gate>();
        RoadMatchDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QSignalSpy ready(&dispatcher, &RoadMatchDispatcher::ready);
        dispatcher.submit({});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        dispatcher.submit({});
        dispatcher.invalidate();
        QVERIFY(!dispatcher.hasPending());
        gate->release.release();
        QTRY_VERIFY(!dispatcher.running());
        QCOMPARE(ready.count(), 0);
        QCOMPARE(gate->calls.load(), 1);
        QCOMPARE(dispatcher.staleCount(), quint64(1));
    }

    void shutdownDoesNotWaitOrRunPending()
    {
        auto gate = std::make_shared<Gate>();
        auto dispatcher = std::make_unique<RoadMatchDispatcher>(nullptr,
            [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        dispatcher->submit({});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QThread *workerThread = gate->workerThread.load();
        QSignalSpy finished(workerThread, &QThread::finished);
        QSignalSpy deleted(workerThread, &QObject::destroyed);
        dispatcher->submit({});
        dispatcher.reset(); // Would deadlock here if destruction joined the worker.
        QCOMPARE(gate->calls.load(), 1);
        gate->release.release();
        QTRY_COMPARE(finished.count(), 1);
        QTRY_COMPARE(deleted.count(), 1);
        QCOMPARE(gate->calls.load(), 1);
    }

    void finalDrainWithoutGuiEvents_data()
    {
        QTest::addColumn<bool>("orphaned");
        QTest::newRow("live service stopped after exec") << false;
        QTest::newRow("previously destroyed service") << true;
    }

    void finalDrainWithoutGuiEvents()
    {
        QFETCH(bool, orphaned);
        QSemaphore entered;
        std::atomic<QThread *> workerThread{nullptr};
        std::atomic<int> calls{0};
        auto dispatcher = std::make_unique<RoadMatchDispatcher>(nullptr,
            [&](const RoadMatchRequest &) {
                workerThread.store(QThread::currentThread());
                ++calls;
                entered.release();
                while (!QThread::currentThread()->isInterruptionRequested())
                    QThread::yieldCurrentThread();
                return RoadMatchResult{};
            });
        dispatcher->submit({});
        QVERIFY(entered.tryAcquire(1, 2000));
        QSignalSpy deleted(workerThread.load(), &QObject::destroyed);
        dispatcher->submit({});
        if (orphaned)
            dispatcher.reset();
        else
            dispatcher->stop();
        RoadWorkerThreads::drainAfterEventLoop();
        // No QTRY/processEvents: draining must not need the departed GUI loop.
        QCOMPARE(deleted.count(), 1);
        QCOMPARE(calls.load(), 1);
        if (dispatcher) {
            QVERIFY(!dispatcher->hasPending());
            dispatcher->submit({}); // stop is terminal, including after drain
            QCOMPARE(calls.load(), 1);
        }
    }

    void startupFailureScopeDrainsWorkers()
    {
        QPointer<QThread> workerThread;
        QSemaphore entered;
        std::atomic<QThread *> observed{nullptr};
        {
            RoadMatchDispatcher dispatcher(nullptr, [&](const auto &) {
                observed.store(QThread::currentThread());
                entered.release();
                return RoadMatchResult{};
            });
            const auto shutdown = qScopeGuard([&]() {
                dispatcher.stop();
                RoadWorkerThreads::drainAfterEventLoop();
            });
            // The production guard is also installed before initialization and
            // exec; an early return must clean workers without event delivery.
            dispatcher.submit({});
            QVERIFY(entered.tryAcquire(1, 2000));
            workerThread = observed.load();
        }
        QVERIFY(workerThread.isNull());
        // A second drain is harmless (normal teardown after an early failure).
        RoadWorkerThreads::drainAfterEventLoop();
    }

    void publicationRequiresCurrentPositionRouteAndMap()
    {
        RoadMatchRequest request;
        request.sequence = 10;
        request.routeGeneration = 20;
        request.mapGeneration = 30;
        QVERIFY(request.isCurrent(10, 20, 30, true));
        QVERIFY(!request.isCurrent(11, 20, 30, true));
        QVERIFY(!request.isCurrent(10, 21, 30, true));
        QVERIFY(!request.isCurrent(10, 20, 31, true));
        QVERIFY(!request.isCurrent(10, 20, 30, false));
    }

    void matcherPreservesMetadataAndSnapshotOwnership()
    {
        VectorTile::Feature feature;
        feature.type = 2;
        feature.geometry = {9, 2048, 4096, 10, 4096, 0};
        feature.properties = {{"kind", "residential"}, {"name", "Test road"},
                              {"ref", " A; B "}, {"maxspeed", "30"},
                              {"route_networks", "DE:regional"}};
        VectorTile::Layer layer;
        layer.name = "streets";
        layer.features.append(feature);
        VectorTile::Tile tile;
        tile.layers.append(layer);
        RoadMatchRequest request;
        request.heading = 90;
        request.headingReliable = true;
        request.lon = 0.010986328125;
        request.lat = std::atan(std::sinh(M_PI / 16384.0)) * 180.0 / M_PI;
        request.tiles.insert((quint64(8192) << 32) | 8192, tile);
        tile.layers[0].features[0].properties["name"] = "Mutated GUI cache";
        const auto result = matchRoad(request);
        QVERIFY(result.selection.index >= 0);
        QVERIFY(result.selection.confident);
        QCOMPARE(result.chosen.name, QString("Test road"));
        QCOMPARE(result.chosen.refs, QString("A, B"));
        QCOMPARE(result.chosen.maxspeed, QString("30"));
        QCOMPARE(result.chosen.routeNetworks, QString("DE:regional"));
        QVERIFY(result.chosen.actualDistanceMeters < 0.01);
        request.tiles.clear();
        request.waitingForTiles = true;
        QVERIFY(matchRoad(request).waitingForTiles);
        request.waitingForTiles = false;
        const auto missing = matchRoad(request);
        QVERIFY(!missing.waitingForTiles);
        QCOMPARE(missing.selection.index, -1);
    }
};
QTEST_GUILESS_MAIN(RoadMatchDispatcherTest)
#include "RoadMatchDispatcherTest.moc"
