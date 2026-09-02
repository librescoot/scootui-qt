#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/VehicleStore.h"

class SyncableStoreSeedTest : public QObject
{
    Q_OBJECT

private slots:
    void startSeedsFromExistingCache();
    void startWithEmptyCacheKeepsDefaults();
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

QTEST_GUILESS_MAIN(SyncableStoreSeedTest)
#include "SyncableStoreSeedTest.moc"
