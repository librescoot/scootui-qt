#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/VehicleStore.h"

class SyncableStoreSeedTest : public QObject
{
    Q_OBJECT

private slots:
    void startSeedsFromExistingCache();
    void startWithEmptyCacheKeepsDefaults();
    void inMemoryReportsSeeded();
    void blinkerUsesSnapshotAnchorOnStartupAndRestart();
    void blinkerWaitsForAnchor();
};

void SyncableStoreSeedTest::startSeedsFromExistingCache()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);

    VehicleStore store(&repo);
    QCOMPARE(store.stateRaw(), QString());
    store.start();
    QCOMPARE(store.stateRaw(), QStringLiteral("ready-to-drive"));
}

void SyncableStoreSeedTest::startWithEmptyCacheKeepsDefaults()
{
    InMemoryMdbRepository repo;
    VehicleStore store(&repo);
    store.start();
    QCOMPARE(store.stateRaw(), QString());
}

void SyncableStoreSeedTest::inMemoryReportsSeeded()
{
    InMemoryMdbRepository repo;
    QVERIFY(repo.isDataSeeded());
}

void SyncableStoreSeedTest::blinkerUsesSnapshotAnchorOnStartupAndRestart()
{
    InMemoryMdbRepository repo;
    const auto anchor = [](int phase) {
        return QString::number((QDateTime::currentMSecsSinceEpoch() - phase) * 1000000LL);
    };
    repo.set("vehicle", "blinker:state", "right", false);
    repo.set("vehicle", "blinker:start_nanos", anchor(250), false);
    VehicleStore store(&repo);
    store.start();
    QVERIFY(store.blinkOpacity() > 0.9);

    // Same direction, different hardware cycle: no state-change signal needed.
    QSignalSpy states(&store, &VehicleStore::blinkerStateChanged);
    repo.set("vehicle", "blinker:start_nanos", anchor(650), false);
    repo.requestAll("vehicle");
    QCOMPARE(store.blinkOpacity(), 0.0);
    QCOMPARE(states.count(), 0);
    repo.set("vehicle", "blinker:start_nanos", anchor(250), false);
    repo.requestAll("vehicle");
    QVERIFY(store.blinkOpacity() > 0.9);
    QCOMPARE(states.count(), 0);

    repo.set("vehicle", "blinker:state", "off");
    QCOMPARE(store.blinkOpacity(), 0.0);
}

void SyncableStoreSeedTest::blinkerWaitsForAnchor()
{
    InMemoryMdbRepository repo;
    repo.set("vehicle", "blinker:state", "right", false);
    VehicleStore store(&repo);
    store.start();
    QTest::qWait(100);
    QCOMPARE(store.blinkOpacity(), 0.0);
}

QTEST_GUILESS_MAIN(SyncableStoreSeedTest)
#include "SyncableStoreSeedTest.moc"
