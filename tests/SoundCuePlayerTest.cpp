#include <QtTest>
#include <QMutexLocker>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThread>
#include <atomic>
#include "services/SoundCuePlayer.h"

struct Recording {
    QMutex mutex;
    QList<SoundCue> calls;
    QSemaphore entered, release;
    std::atomic<bool> blockNext{false};
    std::atomic<bool> wrongThread{false};
    std::atomic<int> destroyed{0};
    QThread *gui = QThread::currentThread();
    quint32 mask = (1u << (int(SoundCue::Error) + 1)) - 2;
    QList<SoundCue> recorded() { QMutexLocker lock(&mutex); return calls; }
};
class FakeAudio final : public SoundCueBackend {
public:
    explicit FakeAudio(std::shared_ptr<Recording> data) : d(std::move(data)) { check(); }
    ~FakeAudio() override { check(); ++d->destroyed; }
    void initialize(const QString &) override
    {
        check();
        emit availableCuesChanged(d->mask);
    }
    void play(SoundCue cue) override
    {
        check();
        { QMutexLocker lock(&d->mutex); d->calls.append(cue); }
        if (d->blockNext.exchange(false)) {
            d->entered.release();
            d->release.acquire();
        }
    }
private:
    void check() { if (QThread::currentThread() == d->gui) d->wrongThread = true; }
    std::shared_ptr<Recording> d;
};
class SoundCuePlayerTest : public QObject {
    Q_OBJECT
private slots:
    void dropsBootCallsAndRunsBackendOnWorker()
    {
        auto d = std::make_shared<Recording>();
        SoundCuePlayer p({}, [d]() { return new FakeAudio(d); }, nullptr, 30);
        p.play(SoundCue::Ready);
        QTRY_VERIFY(p.available(SoundCue::Ready));
        QVERIFY(d->recorded().isEmpty());
        p.play(SoundCue::Ready);
        QTRY_COMPARE(d->recorded().size(), 1);
        p.play(SoundCue::Ready); // Same cue still retriggers when delivered separately.
        QTRY_COMPARE(d->recorded().size(), 2);
        p.stop();
        SoundCuePlayer::drainAfterEventLoop();
        QCOMPARE(d->destroyed.load(), 1);
        QVERIFY(!d->wrongThread.load());
    }
    void boundsBlockedBackendWithoutBlockingGui()
    {
        auto d = std::make_shared<Recording>();
        SoundCuePlayer p({}, [d]() { return new FakeAudio(d); }, nullptr, 0);
        auto unblock = qScopeGuard([&]() { d->release.release(); });
        QTRY_VERIFY(p.available(SoundCue::Ready));
        d->blockNext = true;
        p.play(SoundCue::Ready);
        QVERIFY(d->entered.tryAcquire(1, 1000));
        bool guiAlive = false;
        QTimer::singleShot(0, [&]() { guiAlive = true; });
        for (int i = 0; i < 1000; ++i) {
            p.play(SoundCue::BlinkerPulse);
            p.play(SoundCue::Info);
            p.play(SoundCue::Wake);
            p.play(SoundCue::Parked);
        }
        p.play(SoundCue::Shutdown);
        p.play(SoundCue::BlinkerOff);
        QCOMPARE(p.pendingCount(), 3); // info, latest state, latest blinker edge
        QTRY_VERIFY(guiAlive);
        d->release.release();
        QTRY_COMPARE(d->recorded().size(), 4);
        QCOMPARE(d->recorded(), (QList<SoundCue>{SoundCue::Ready, SoundCue::Info,
                                                SoundCue::Shutdown, SoundCue::BlinkerOff}));
        p.stop();
        SoundCuePlayer::drainAfterEventLoop();
        QVERIFY(!d->wrongThread.load());
    }
    void unavailableLatestCueSupersedesPending_data()
    {
        QTest::addColumn<int>("oldCue");
        QTest::addColumn<int>("newCue");
        QTest::newRow("blinker-off") << int(SoundCue::BlinkerPulse) << int(SoundCue::BlinkerOff);
        QTest::newRow("vehicle-shutdown") << int(SoundCue::Wake) << int(SoundCue::Shutdown);
    }
    void unavailableLatestCueSupersedesPending()
    {
        QFETCH(int, oldCue);
        QFETCH(int, newCue);
        auto d = std::make_shared<Recording>();
        d->mask = (1u << int(SoundCue::Ready)) | (1u << oldCue) | (1u << int(SoundCue::Error));
        SoundCuePlayer p({}, [d]() { return new FakeAudio(d); }, nullptr, 0);
        auto unblock = qScopeGuard([&]() { d->release.release(); });
        QTRY_VERIFY(p.available(SoundCue::Ready));
        QVERIFY(!p.available(SoundCue(newCue)));
        d->blockNext = true;
        p.play(SoundCue::Ready);
        QVERIFY(d->entered.tryAcquire(1, 1000));
        p.play(SoundCue(oldCue));
        QCOMPARE(p.pendingCount(), 1);
        p.play(SoundCue(newCue));
        QCOMPARE(p.pendingCount(), 0);
        p.play(SoundCue::Error);
        d->release.release();
        QTRY_COMPARE(d->recorded().size(), 2);
        QCOMPARE(d->recorded(), (QList<SoundCue>{SoundCue::Ready, SoundCue::Error}));
        p.stop();
        SoundCuePlayer::drainAfterEventLoop();
    }
    void dropsExpiredPulseButPreservesOtherCues()
    {
        auto d = std::make_shared<Recording>();
        SoundCuePlayer p({}, [d]() { return new FakeAudio(d); }, nullptr, 0);
        auto unblock = qScopeGuard([&]() { d->release.release(); });
        QTRY_VERIFY(p.available(SoundCue::Ready));
        d->blockNext = true;
        p.play(SoundCue::Ready);
        QVERIFY(d->entered.tryAcquire(1, 1000));
        p.play(SoundCue::BlinkerPulse);
        p.play(SoundCue::Error);
        QTest::qWait(SoundCuePlayer::PulseDeadlineMs + 20);
        d->release.release();
        QTRY_COMPARE(d->recorded().size(), 2);
        QCOMPARE(d->recorded().last(), SoundCue::Error);
        p.stop();
        SoundCuePlayer::drainAfterEventLoop();
    }
    void destroyedOwnerDoesNotWaitOrReplayPending()
    {
        auto d = std::make_shared<Recording>();
        auto *p = new SoundCuePlayer({}, [d]() { return new FakeAudio(d); }, nullptr, 0);
        auto unblock = qScopeGuard([&]() { d->release.release(); });
        QTRY_VERIFY(p->available(SoundCue::Ready));
        d->blockNext = true;
        p->play(SoundCue::Ready);
        QVERIFY(d->entered.tryAcquire(1, 1000));
        p->play(SoundCue::Shutdown);
        QElapsedTimer elapsed; elapsed.start();
        delete p;
        QVERIFY(elapsed.elapsed() < 100);
        QCOMPARE(d->destroyed.load(), 0);
        d->release.release();
        // Simulates post-exec drain: no GUI event delivery is needed.
        SoundCuePlayer::drainAfterEventLoop();
        QCOMPARE(d->destroyed.load(), 1);
        QCOMPARE(d->recorded().size(), 1);
        QVERIFY(!d->wrongThread.load());
    }
    void cleanup() { SoundCuePlayer::drainAfterEventLoop(); }
};
QTEST_GUILESS_MAIN(SoundCuePlayerTest)
#include "SoundCuePlayerTest.moc"
