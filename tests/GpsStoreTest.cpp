#include <QtTest>

#include "repositories/InMemoryMdbRepository.h"
#include "stores/GpsStore.h"

class GpsStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void snapshotIsPublishedAtomically();
    void recentFixExpiresWithNotification();
    void rejectsIncompleteAndOutOfRangeCoordinates();
};

void GpsStoreTest::snapshotIsPublishedAtomically()
{
    InMemoryMdbRepository repo;
    GpsStore store(&repo);
    store.start();

    int samples = 0;
    QList<GpsSample> observed;
    connect(&store, &GpsStore::sampleChanged, this, [&]() {
        ++samples;
        observed.append(store.currentSample());
    });

    repo.publish(QStringLiteral("gps:tpv"), QStringLiteral(
        R"({"latitude":"52.500001","longitude":"13.400002","course":"87.5","speed":"31.2","eph":"4.5","hdop":"0.8","state":"fix-established","timestamp":"2026-08-24T10:00:00Z"})"));

    QCOMPARE(samples, 1);
    QCOMPARE(observed.size(), 1);
    QCOMPARE(observed.first().latitude, 52.500001);
    QCOMPARE(observed.first().longitude, 13.400002);
    QCOMPARE(observed.first().course, 87.5);
    QCOMPARE(observed.first().speedKmh, 31.2);
    QVERIFY(observed.first().hasValidCoordinate());
    QVERIFY(store.timestampAgeMs() < 100);
}

void GpsStoreTest::recentFixExpiresWithNotification()
{
    InMemoryMdbRepository repo;
    GpsStore store(&repo, nullptr, 30);
    store.start();
    QSignalSpy recentSpy(&store, &GpsStore::recentFixChanged);

    repo.publish(QStringLiteral("gps:tpv"), QStringLiteral(
        R"({"latitude":"52.5","longitude":"13.4","state":"fix-established","timestamp":"2026-08-24T10:00:01Z"})"));

    QVERIFY(store.hasRecentFix());
    QTRY_VERIFY_WITH_TIMEOUT(recentSpy.count() >= 2, 250); // became recent, then expired
    QVERIFY(!store.hasRecentFix());
}

void GpsStoreTest::rejectsIncompleteAndOutOfRangeCoordinates()
{
    GpsSample sample;
    sample.timestamp = QStringLiteral("2026-08-24T10:00:00Z");
    sample.latitude = 52.5;
    sample.longitude = 181.0;
    QVERIFY(!sample.hasValidCoordinate());

    sample.longitude = 13.4;
    QVERIFY(sample.hasValidCoordinate());

    sample.timestamp.clear();
    QVERIFY(!sample.hasValidCoordinate());
}

QTEST_MAIN(GpsStoreTest)
#include "GpsStoreTest.moc"
