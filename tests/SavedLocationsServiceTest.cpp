#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "services/SavedLocationsService.h"

class SavedLocationsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void persistsMetadataAndResolvesSlotConflicts();
    void saveResolvesConflictWithoutHiddenReactivation();
    void deletionClearsQuickMetadata();
    void movingSlotSwapsWithTheHolder();
    void clearingSlotLeavesOthersAlone();
    void migratesExistingLocationUuid();
};

static SavedLocation location(double latitude, double longitude, const QString &label)
{
    SavedLocation result;
    result.latitude = latitude;
    result.longitude = longitude;
    result.label = label;
    return result;
}

void SavedLocationsServiceTest::persistsMetadataAndResolvesSlotConflicts()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Home address"))));
    QVERIFY(service.save(location(52.6, 13.5, QStringLiteral("Work address"))));

    QVERIFY(service.setQuickSlot(0, 1));
    QVERIFY(service.setQuickIcon(0, QStringLiteral("home")));
    QVERIFY(service.setQuickSlot(1, 1));

    auto saved = service.loadAll();
    QCOMPARE(saved.size(), 2);
    QCOMPARE(saved[0].quickSlot, 0);
    QCOMPARE(saved[0].quickIcon, QStringLiteral("home"));
    QCOMPARE(saved[1].quickSlot, 1);

    QVERIFY(service.setQuickSlot(0, 2));
    QVERIFY(service.setQuickSlot(1, 2));
    saved = service.loadAll();
    QCOMPARE(saved[0].quickSlot, 1);
    QCOMPARE(saved[1].quickSlot, 2);

    int assigned = 0;
    for (const auto &entry : saved)
        assigned += entry.quickSlot > 0 ? 1 : 0;
    QCOMPARE(assigned, 2);
}

void SavedLocationsServiceTest::saveResolvesConflictWithoutHiddenReactivation()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);

    SavedLocation first = location(52.5, 13.4, QStringLiteral("First"));
    first.quickSlot = 1;
    QVERIFY(service.save(first));

    SavedLocation second = location(52.6, 13.5, QStringLiteral("Second"));
    second.quickSlot = 1;
    QVERIFY(service.save(second));

    QCOMPARE(repo.get(QStringLiteral("settings"),
                      QStringLiteral("dashboard.saved-locations.0.quick-slot")),
             QStringLiteral("0"));
    QCOMPARE(repo.get(QStringLiteral("settings"),
                      QStringLiteral("dashboard.saved-locations.1.quick-slot")),
             QStringLiteral("1"));

    QVERIFY(service.remove(1));
    const auto remaining = service.loadAll();
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining[0].id, 0);
    QCOMPARE(remaining[0].quickSlot, 0);
}

int idOf(const QList<SavedLocation> &locations, const QString &label)
{
    for (const auto &entry : locations)
        if (entry.label == label)
            return entry.id;
    return -1;
}

int slotOf(const QList<SavedLocation> &locations, const QString &label)
{
    for (const auto &entry : locations)
        if (entry.label == label)
            return entry.quickSlot;
    return -1;
}

// Choosing the holder of slot 2 for slot 1 swaps them rather than dropping one.
void SavedLocationsServiceTest::movingSlotSwapsWithTheHolder()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Home"))));
    QVERIFY(service.save(location(52.6, 13.5, QStringLiteral("Work"))));

    QList<SavedLocation> saved = service.loadAll();
    const int home = idOf(saved, QStringLiteral("Home"));
    const int work = idOf(saved, QStringLiteral("Work"));
    QVERIFY(home >= 0 && work >= 0);
    QVERIFY(service.setQuickSlot(home, 1));
    QVERIFY(service.setQuickSlot(work, 2));

    QVERIFY(service.setQuickSlot(work, 1));
    saved = service.loadAll();
    QCOMPARE(slotOf(saved, QStringLiteral("Work")), 1);
    QCOMPARE(slotOf(saved, QStringLiteral("Home")), 2);
}

void SavedLocationsServiceTest::clearingSlotLeavesOthersAlone()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Home"))));
    QVERIFY(service.save(location(52.6, 13.5, QStringLiteral("Work"))));

    const int home = idOf(service.loadAll(), QStringLiteral("Home"));
    const int work = idOf(service.loadAll(), QStringLiteral("Work"));
    QVERIFY(service.setQuickSlot(home, 1));
    QVERIFY(service.setQuickSlot(work, 2));

    QVERIFY(service.setQuickSlot(home, 0));
    const QList<SavedLocation> saved = service.loadAll();
    QCOMPARE(slotOf(saved, QStringLiteral("Home")), 0);
    QCOMPARE(slotOf(saved, QStringLiteral("Work")), 2);
}

void SavedLocationsServiceTest::deletionClearsQuickMetadata()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Old"))));
    const QString oldUuid = service.loadAll().at(0).uuid;
    QVERIFY(!oldUuid.isEmpty());
    QVERIFY(service.setQuickSlot(0, 1));
    QVERIFY(service.setQuickIcon(0, QStringLiteral("favorite")));
    QVERIFY(service.remove(0));

    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-slot")).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-icon")).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.uuid")).isEmpty());

    QVERIFY(service.save(location(51.0, 12.0, QStringLiteral("New"))));
    const auto saved = service.loadAll();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved[0].id, 0);
    QCOMPARE(saved[0].quickSlot, 0);
    QCOMPARE(saved[0].quickIcon, QStringLiteral("place"));
    QVERIFY(saved[0].uuid != oldUuid);
}

void SavedLocationsServiceTest::migratesExistingLocationUuid()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.saved-locations.4.latitude"),
             QStringLiteral("52.5200000"), false);
    repo.set(QStringLiteral("settings"), QStringLiteral("dashboard.saved-locations.4.longitude"),
             QStringLiteral("13.4050000"), false);

    SavedLocationsService service(&repo);
    const QList<SavedLocation> first = service.loadAll();
    QCOMPARE(first.size(), 1);
    QVERIFY(!first[0].uuid.isEmpty());
    QCOMPARE(repo.get(QStringLiteral("settings"),
                      QStringLiteral("dashboard.saved-locations.4.uuid")), first[0].uuid);

    const QList<SavedLocation> second = service.loadAll();
    QCOMPARE(second.at(0).uuid, first[0].uuid);
}

QTEST_GUILESS_MAIN(SavedLocationsServiceTest)
#include "SavedLocationsServiceTest.moc"
