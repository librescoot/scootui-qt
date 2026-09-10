#include <QtTest>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <memory>
#include "services/StreetQueryDispatcher.h"
#include "services/RoadWorkerThreads.h"

namespace {
struct Gate {
    QSemaphore entered, release;
    std::atomic<int> calls{0};
    std::atomic<QThread *> worker{nullptr};
    StreetQueryResult compute(const StreetQueryRequest &request) {
        worker = QThread::currentThread();
        ++calls;
        entered.release();
        release.acquire();
        return {{request.minLat, request.mapGeneration, request.path}, true};
    }
};
struct ReleaseOnExit {
    std::shared_ptr<Gate> gate;
    ~ReleaseOnExit() { gate->release.release(100); }
};
}

class StreetQueryDispatcherTest : public QObject
{
    Q_OBJECT
private slots:
    void latestWinsAndBoundsPendingAndReplies() {
        auto gate = std::make_shared<Gate>();
        StreetQueryDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QObject owner;
        QSignalSpy ready(&dispatcher, &StreetQueryDispatcher::ready);
        dispatcher.submit(&owner, {});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QVERIFY(gate->worker.load() != thread());
        StreetQueryRequest r;
        r.mapGeneration = 3; r.path = "new.mbtiles";
        quint64 latest = 0;
        for (int i = 0; i < 100; ++i) {
            r.minLat = i;
            latest = dispatcher.submit(&owner, r);
        }
        QCOMPARE(gate->calls.load(), 1);
        QVERIFY(dispatcher.hasPending());
        bool guiRan = false;
        QTimer::singleShot(0, this, [&]() { guiRan = true; });
        QTRY_VERIFY(guiRan);
        gate->release.release();
        QTRY_COMPARE(gate->calls.load(), 2);
        QCOMPARE(ready.count(), 0);
        QVERIFY(!dispatcher.hasPending());
        gate->release.release();
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(ready[0][0].value<QObject *>(), &owner);
        QCOMPARE(ready[0][1].toULongLong(), latest);
        QCOMPARE(ready[0][2].toList(), (QVariantList{99.0, 3, "new.mbtiles"}));
        QVERIFY(!dispatcher.running());
    }

    void invalidationAndOwnerDestruction_data() {
        QTest::addColumn<int>("operation");
        QTest::newRow("route-map-invalidate") << 0;
        QTest::newRow("explicit-cancel") << 1;
        QTest::newRow("owner-destroyed") << 2;
    }
    void invalidationAndOwnerDestruction() {
        QFETCH(int, operation);
        auto gate = std::make_shared<Gate>();
        StreetQueryDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        auto owner = std::make_unique<QObject>();
        QSignalSpy ready(&dispatcher, &StreetQueryDispatcher::ready);
        dispatcher.submit(owner.get(), {});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        dispatcher.submit(owner.get(), {});
        if (operation == 0) dispatcher.invalidate();
        if (operation == 1) dispatcher.cancel(owner.get());
        if (operation == 2) owner.reset();
        QVERIFY(!dispatcher.hasPending());
        gate->release.release();
        QTRY_VERIFY(!dispatcher.running());
        QCOMPARE(gate->calls.load(), 1);
        QCOMPARE(ready.count(), 0);
    }

    void oldOwnerCannotCancelReplacement() {
        auto gate = std::make_shared<Gate>();
        StreetQueryDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        auto oldOwner = std::make_unique<QObject>();
        QObject current;
        QSignalSpy ready(&dispatcher, &StreetQueryDispatcher::ready);
        dispatcher.submit(oldOwner.get(), {});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        dispatcher.submit(&current, {});
        dispatcher.cancel(oldOwner.get());
        oldOwner.reset();
        gate->release.release(2);
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(ready[0][0].value<QObject *>(), &current);
    }

    void destructionDoesNotWaitForStorage() {
        auto gate = std::make_shared<Gate>();
        auto dispatcher = std::make_unique<StreetQueryDispatcher>(nullptr,
            [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QObject owner;
        dispatcher->submit(&owner, {});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        QElapsedTimer timer;
        timer.start();
        dispatcher.reset();
        QVERIFY(timer.elapsed() < 100);
        gate->release.release();
    }

    void prefetchPromotionCacheAndPriority() {
        auto gate = std::make_shared<Gate>();
        StreetQueryDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QObject owner;
        QSignalSpy ready(&dispatcher, &StreetQueryDispatcher::ready);
        StreetQueryRequest r;
        r.geometryKey = "turn1";
        dispatcher.prefetch(r);
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        const auto visibleSequence = dispatcher.submit(&owner, r);
        QVERIFY(!dispatcher.hasPending());
        StreetQueryRequest speculative = r;
        speculative.geometryKey = "turn2";
        for (int i = 0; i < 100; ++i) dispatcher.prefetch(speculative);
        QVERIFY(!dispatcher.hasPending());
        gate->release.release();
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(ready[0][1].toULongLong(), visibleSequence);
        QCOMPARE(gate->calls.load(), 1);
        QVERIFY(dispatcher.cached(r).complete);
        QVERIFY(!dispatcher.cached(speculative).complete);
        dispatcher.cancel(&owner);
        dispatcher.submit(&owner, r);
        QTRY_COMPARE(ready.count(), 2);
        QCOMPARE(gate->calls.load(), 1); // completed prefetch reused without I/O
        dispatcher.invalidate();
        QVERIFY(!dispatcher.cached(r).complete);
        dispatcher.prefetch(r);
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        // Visible demand replaces obsolete pending speculation, never queues jobs.
        dispatcher.prefetch(speculative);
        StreetQueryRequest visible = r;
        visible.mapGeneration = 2;
        dispatcher.submit(&owner, visible);
        for (int i = 0; i < 100; ++i) dispatcher.prefetch(speculative);
        gate->release.release();
        QTRY_COMPARE(gate->calls.load(), 3);
        QCOMPARE(ready.count(), 2);
        gate->release.release();
        QTRY_COMPARE(ready.count(), 3);
        QCOMPARE(ready[2][2].toList()[1].toInt(), 2);
        QVERIFY(dispatcher.cached(visible).complete);
        QVERIFY(!dispatcher.cached(r).complete); // single-entry cache
    }

    void prefetchInvalidationRejectsIdenticalOldQuery() {
        auto gate = std::make_shared<Gate>();
        StreetQueryDispatcher dispatcher(nullptr, [gate](const auto &r) { return gate->compute(r); });
        ReleaseOnExit cleanup{gate};
        QObject owner;
        QSignalSpy ready(&dispatcher, &StreetQueryDispatcher::ready);
        dispatcher.prefetch({});
        QVERIFY(gate->entered.tryAcquire(1, 2000));
        dispatcher.invalidate();
        dispatcher.submit(&owner, {});
        QVERIFY(dispatcher.hasPending());
        gate->release.release();
        QTRY_COMPARE(gate->calls.load(), 2);
        QCOMPARE(ready.count(), 0);
        QVERIFY(!dispatcher.cached({}).complete);
        gate->release.release();
        QTRY_COMPARE(ready.count(), 1);
    }

    void reentrantSubmitStaysBounded() {
        StreetQueryDispatcher dispatcher(nullptr, [](const auto &) { return StreetQueryResult{}; });
        QObject owner;
        int replies = 0;
        connect(&dispatcher, &StreetQueryDispatcher::ready, this, [&]() {
            QCOMPARE(QThread::currentThread(), thread());
            if (++replies == 1) dispatcher.submit(&owner, {});
        });
        dispatcher.submit(&owner, {});
        QTRY_COMPARE(replies, 2);
        QVERIFY(!dispatcher.running());
    }
    void cleanupTestCase() { RoadWorkerThreads::drainAfterEventLoop(); }
};
QTEST_GUILESS_MAIN(StreetQueryDispatcherTest)
#include "StreetQueryDispatcherTest.moc"
