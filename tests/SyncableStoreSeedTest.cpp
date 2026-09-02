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

QTEST_GUILESS_MAIN(SyncableStoreSeedTest)
#include "SyncableStoreSeedTest.moc"
