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

QTEST_GUILESS_MAIN(MapMetadataTest)
#include "MapMetadataTest.moc"
