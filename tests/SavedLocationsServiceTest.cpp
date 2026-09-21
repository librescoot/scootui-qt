#include <QtTest>

#include "core/ShortcutMenuItems.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/SavedLocationsService.h"

class SavedLocationsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void saveRoundTripsWithoutQuickSlotWrites();
    void deletionClearsAllRecordFields();
    void migratesExistingLocationUuid();
    void legacyQuickAssignmentsOrderBySlot();
    void removePrunesDestinationItem();
};

static SavedLocation location(double latitude, double longitude, const QString &label)
{
    SavedLocation result;
    result.latitude = latitude;
    result.longitude = longitude;
    result.label = label;
    return result;
}

void SavedLocationsServiceTest::saveRoundTripsWithoutQuickSlotWrites()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Home address"))));

    const auto saved = service.loadAll();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved[0].label, QStringLiteral("Home address"));
    QVERIFY(!saved[0].uuid.isEmpty());
    QCOMPARE(repo.get(QStringLiteral("settings"),
                      QStringLiteral("dashboard.saved-locations.0.uuid")),
             saved[0].uuid);
    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-slot")).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"),
                     QStringLiteral("dashboard.saved-locations.0.quick-icon")).isEmpty());
}

void SavedLocationsServiceTest::deletionClearsAllRecordFields()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Old"))));
    const QString oldUuid = service.loadAll().at(0).uuid;
    QVERIFY(!oldUuid.isEmpty());

    repo.set(QStringLiteral("settings"),
             QStringLiteral("dashboard.saved-locations.0.quick-slot"),
             QStringLiteral("1"), false);
    repo.set(QStringLiteral("settings"),
             QStringLiteral("dashboard.saved-locations.0.quick-icon"),
             QStringLiteral("favorite"), false);

    QVERIFY(service.remove(0));
    for (const QString &field : {QStringLiteral("latitude"), QStringLiteral("longitude"),
                                 QStringLiteral("label"), QStringLiteral("quick-slot"),
                                 QStringLiteral("quick-icon"), QStringLiteral("uuid"),
                                 QStringLiteral("created-at"),
                                 QStringLiteral("last-used-at")}) {
        QVERIFY2(repo.get(QStringLiteral("settings"),
                          QStringLiteral("dashboard.saved-locations.0.%1").arg(field))
                     .isEmpty(),
                 qPrintable(field));
    }

    QVERIFY(service.save(location(51.0, 12.0, QStringLiteral("New"))));
    const auto saved = service.loadAll();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved[0].id, 0);
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

void SavedLocationsServiceTest::legacyQuickAssignmentsOrderBySlot()
{
    InMemoryMdbRepository repo;
    const auto seed = [&repo](int id, const QString &slot, const QString &uuid,
                              const QString &icon, bool withCoords = true) {
        const QString prefix = QStringLiteral("dashboard.saved-locations.%1.").arg(id);
        if (withCoords) {
            repo.set(QStringLiteral("settings"), prefix + QStringLiteral("latitude"),
                     QStringLiteral("52.5000000"), false);
            repo.set(QStringLiteral("settings"), prefix + QStringLiteral("longitude"),
                     QStringLiteral("13.4000000"), false);
        }
        repo.set(QStringLiteral("settings"), prefix + QStringLiteral("quick-slot"), slot, false);
        if (!uuid.isEmpty())
            repo.set(QStringLiteral("settings"), prefix + QStringLiteral("uuid"), uuid, false);
        if (!icon.isEmpty())
            repo.set(QStringLiteral("settings"), prefix + QStringLiteral("quick-icon"), icon, false);
    };

    const QString uuidA = QStringLiteral("3fa85f64-5717-4562-b3fc-2c963f66afa6");
    const QString uuidB = QStringLiteral("0d6c21f0-0000-4000-8000-000000000001");
    seed(0, QStringLiteral("2"), uuidA, QStringLiteral("home"));
    seed(1, QStringLiteral("1"), uuidB, QString());
    // Same uuid in two slots keeps the first occurrence only.
    seed(2, QStringLiteral("1"), uuidB, QStringLiteral("work"));
    // Unassigned, uuid-less, and coordinate-less records stay out.
    seed(3, QStringLiteral("0"), QStringLiteral("2d5a1c1e-1111-4222-8333-444455556666"),
         QString());
    seed(4, QStringLiteral("1"), QString(), QString());
    seed(5, QStringLiteral("1"),
         QStringLiteral("6f0e1f7c-aaaa-4bbb-8ccc-ddddeeeeffff"), QString(), false);

    SavedLocationsService service(&repo);
    const auto assignments = service.loadLegacyQuickAssignments();
    QCOMPARE(assignments.size(), 2);
    QCOMPARE(assignments.at(0).id, 1);
    QCOMPARE(assignments.at(0).slot, 1);
    QCOMPARE(assignments.at(0).icon, QStringLiteral("place"));
    QCOMPARE(assignments.at(0).uuid, uuidB);
    QCOMPARE(assignments.at(1).id, 0);
    QCOMPARE(assignments.at(1).slot, 2);
    QCOMPARE(assignments.at(1).icon, QStringLiteral("home"));
    QCOMPARE(assignments.at(1).uuid, uuidA);
}

void SavedLocationsServiceTest::removePrunesDestinationItem()
{
    InMemoryMdbRepository repo;
    SavedLocationsService service(&repo);
    QVERIFY(service.save(location(52.5, 13.4, QStringLiteral("Home"))));
    QVERIFY(service.save(location(52.6, 13.5, QStringLiteral("Work"))));
    const auto saved = service.loadAll();
    QCOMPARE(saved.size(), 2);

    QStringList items = ShortcutMenuItems::defaultItems();
    items.append(ShortcutMenuItems::destinationToken(saved[0].uuid, QStringLiteral("home")));
    items.append(ShortcutMenuItems::destinationToken(saved[1].uuid, QStringLiteral("work")));
    repo.set(QStringLiteral("settings"), QLatin1String(ShortcutMenuItems::SettingsKey),
             ShortcutMenuItems::serialize(items), false);

    QVERIFY(service.remove(0));
    bool ok = false;
    const QStringList afterRemove = ShortcutMenuItems::parse(
        repo.get(QStringLiteral("settings"), QLatin1String(ShortcutMenuItems::SettingsKey)), &ok);
    QVERIFY(ok);
    QCOMPARE(ShortcutMenuItems::destinationUuids(afterRemove),
             QStringList{saved[1].uuid});
}

QTEST_GUILESS_MAIN(SavedLocationsServiceTest)
#include "SavedLocationsServiceTest.moc"
