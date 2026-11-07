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
    void regionNameRoundTrip();
    void versionsRoundTrip();
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

void MapMetadataTest::regionNameRoundTrip()
{
    MapMetadata meta;
    meta.region = QStringLiteral("zurich");
    meta.regionName = QStringLiteral("Kanton Zürich");
    const auto restored = MapMetadata::fromJson(meta.toJson());
    QCOMPARE(restored.region, meta.region);
    QCOMPARE(restored.regionName, meta.regionName);

    const auto older = MapMetadata::fromJson(QJsonObject{
        {QStringLiteral("region"), QStringLiteral("bayern")},
    });
    QVERIFY(older.regionName.isEmpty());
}

void MapMetadataTest::versionsRoundTrip()
{
    MapMetadata meta;
    meta.displayTiles = MapTileInfo{};
    meta.displayTiles->version = QStringLiteral("tiles-20261001T094148Z");
    meta.valhallaTiles = MapTileInfo{};
    meta.valhallaTiles->version = QStringLiteral("tiles-20261101T130555Z");
    const auto restored = MapMetadata::fromJson(meta.toJson());
    QVERIFY(restored.displayTiles.has_value());
    QVERIFY(restored.valhallaTiles.has_value());
    QCOMPARE(restored.displayTiles->version, meta.displayTiles->version);
    QCOMPARE(restored.valhallaTiles->version, meta.valhallaTiles->version);
    QVERIFY(MapTileInfo::fromJson(QJsonObject{}).version.isEmpty());
}

QTEST_GUILESS_MAIN(MapMetadataTest)
#include "MapMetadataTest.moc"
