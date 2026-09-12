#include <QtTest>

#include "models/MapMetadata.h"

// The per-set update flags ride the same metadata.json as the combined flag;
// the maps hash publishes them so consumers can name the stale tile set.
class MapMetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void perSetFlagsRoundTrip();
    void falseFlagsAreOmitted();
    void legacyCombinedFlagWidensToBothSets();
};

void MapMetadataTest::perSetFlagsRoundTrip()
{
    MapMetadata meta;
    meta.region = QStringLiteral("berlin_brandenburg");
    meta.displayUpdateAvailable = true;

    MapMetadata back = MapMetadata::fromJson(MapMetadata(meta).toJson());
    QCOMPARE(back.displayUpdateAvailable, true);
    QCOMPARE(back.routingUpdateAvailable, false);
    QCOMPARE(back.updateAvailable, false);
}

void MapMetadataTest::falseFlagsAreOmitted()
{
    // Same shape the pre-per-set writers produced: absent keys, not "false".
    MapMetadata meta;
    meta.region = QStringLiteral("berlin_brandenburg");
    QJsonObject o = MapMetadata(meta).toJson();
    QVERIFY(!o.contains(QStringLiteral("updateAvailable")));
    QVERIFY(!o.contains(QStringLiteral("displayUpdateAvailable")));
    QVERIFY(!o.contains(QStringLiteral("routingUpdateAvailable")));
}

void MapMetadataTest::legacyCombinedFlagWidensToBothSets()
{
    // Shape a pre-per-set writer produced: combined flag only.
    MapMetadata legacy = MapMetadata::fromJson(QJsonObject{
        {QStringLiteral("region"), QStringLiteral("berlin_brandenburg")},
        {QStringLiteral("updateAvailable"), true},
    });
    QCOMPARE(legacy.displayUpdateAvailable, false);

    legacy.normaliseUpdateTargets();
    QCOMPARE(legacy.displayUpdateAvailable, true);
    QCOMPARE(legacy.routingUpdateAvailable, true);

    // A current writer that named one set must not be widened.
    MapMetadata current = MapMetadata::fromJson(QJsonObject{
        {QStringLiteral("updateAvailable"), true},
        {QStringLiteral("displayUpdateAvailable"), true},
    });
    current.normaliseUpdateTargets();
    QCOMPARE(current.displayUpdateAvailable, true);
    QCOMPARE(current.routingUpdateAvailable, false);

    // Nothing to update stays nothing to update.
    MapMetadata idle;
    idle.normaliseUpdateTargets();
    QCOMPARE(idle.displayUpdateAvailable, false);
    QCOMPARE(idle.routingUpdateAvailable, false);
}

QTEST_GUILESS_MAIN(MapMetadataTest)
#include "MapMetadataTest.moc"
