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

void SavedLocationsServiceTest::deletionClearsQuickMetadata()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Old"))));
    QVERIFY(service.setQuickSlot(0, 1));
    QVERIFY(service.setQuickIcon(0, QStringLiteral("favorite")));
    QVERIFY(service.remove(0));

    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-slot")).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-icon")).isEmpty());

    QVERIFY(service.save(location(51.0, 12.0, QStringLiteral("New"))));
    const auto saved = service.loadAll();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved[0].id, 0);
    QCOMPARE(saved[0].quickSlot, 0);
    QCOMPARE(saved[0].quickIcon, QStringLiteral("place"));
}

QTEST_GUILESS_MAIN(SavedLocationsServiceTest)
#include "SavedLocationsServiceTest.moc"
