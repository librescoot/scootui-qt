#include <QtTest>

#include "services/MapPlanGeometry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// GeoJSON encoding for the multi-hop plan overlay. Kept independent of
// MapService, so the wire shape the map layers consume is pinned here.
class MapPlanGeometryTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyLineIsAnEmptyCollection();
    void lineUsesLongitudeLatitudeOrder();
    void stopsCarryIndexCurrentAndReached();
    void stopsSkipInvalidCoordinates();
    void emptyStopsIsAnEmptyCollection();
};

namespace {

QJsonObject parse(const QString &json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

QVariantList stop(double lat, double lon, const QString &label, bool reached = false)
{
    QVariantMap map;
    map[QStringLiteral("latitude")] = lat;
    map[QStringLiteral("longitude")] = lon;
    map[QStringLiteral("label")] = label;
    map[QStringLiteral("reached")] = reached;
    return {map};
}

} // namespace

void MapPlanGeometryTest::emptyLineIsAnEmptyCollection()
{
    const QJsonObject root = parse(MapPlanGeometry::lineGeoJson({}));
    QCOMPARE(root.value(QStringLiteral("type")).toString(), QStringLiteral("FeatureCollection"));
    QVERIFY(root.value(QStringLiteral("features")).toArray().isEmpty());

    // A single point is not a line.
    const QJsonObject single = parse(MapPlanGeometry::lineGeoJson({{52.5, 13.4}}));
    QVERIFY(single.value(QStringLiteral("features")).toArray().isEmpty());
}

void MapPlanGeometryTest::lineUsesLongitudeLatitudeOrder()
{
    const QList<LatLng> points = {{52.50, 13.40}, {52.51, 13.41}};
    const QJsonObject root = parse(MapPlanGeometry::lineGeoJson(points));
    QCOMPARE(root.value(QStringLiteral("type")).toString(), QStringLiteral("Feature"));

    const QJsonObject geometry = root.value(QStringLiteral("geometry")).toObject();
    QCOMPARE(geometry.value(QStringLiteral("type")).toString(), QStringLiteral("LineString"));
    const QJsonArray coordinates = geometry.value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(coordinates.size(), 2);
    // GeoJSON is [longitude, latitude].
    QCOMPARE(coordinates.at(0).toArray().at(0).toDouble(), 13.40);
    QCOMPARE(coordinates.at(0).toArray().at(1).toDouble(), 52.50);
    QCOMPARE(coordinates.at(1).toArray().at(0).toDouble(), 13.41);
    QCOMPARE(coordinates.at(1).toArray().at(1).toDouble(), 52.51);
}

void MapPlanGeometryTest::stopsCarryIndexCurrentAndReached()
{
    QVariantList stops;
    stops += stop(52.51, 13.41, QStringLiteral("A"), true);
    stops += stop(52.52, 13.42, QStringLiteral("B"));
    stops += stop(52.53, 13.43, QStringLiteral("C"));

    const QJsonObject root = parse(MapPlanGeometry::stopsGeoJson(stops, 1));
    QCOMPARE(root.value(QStringLiteral("type")).toString(), QStringLiteral("FeatureCollection"));
    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 3);

    const QJsonObject first = features.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("geometry")).toObject()
                 .value(QStringLiteral("type")).toString(), QStringLiteral("Point"));
    const QJsonArray coords = first.value(QStringLiteral("geometry")).toObject()
                                  .value(QStringLiteral("coordinates")).toArray();
    QCOMPARE(coords.at(0).toDouble(), 13.41);
    QCOMPARE(coords.at(1).toDouble(), 52.51);

    const QJsonObject firstProps = first.value(QStringLiteral("properties")).toObject();
    QCOMPARE(firstProps.value(QStringLiteral("index")).toInt(), 0);
    QCOMPARE(firstProps.value(QStringLiteral("current")).toInt(), 0);
    QCOMPARE(firstProps.value(QStringLiteral("reached")).toInt(), 1);
    QCOMPARE(firstProps.value(QStringLiteral("first")).toInt(), 1);
    QCOMPARE(firstProps.value(QStringLiteral("last")).toInt(), 0);
    QCOMPARE(firstProps.value(QStringLiteral("label")).toString(), QStringLiteral("A"));

    QCOMPARE(features.at(1).toObject().value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("current")).toInt(), 1);
    QCOMPARE(features.at(2).toObject().value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("current")).toInt(), 0);
    QCOMPARE(features.at(2).toObject().value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("last")).toInt(), 1);
}

void MapPlanGeometryTest::stopsSkipInvalidCoordinates()
{
    QVariantList stops = stop(52.51, 13.41, QStringLiteral("A"));
    stops += stop(0.0, 0.0, QStringLiteral("origin"));
    stops += stop(52.52, 13.42, QStringLiteral("B"));

    const QJsonObject root = parse(MapPlanGeometry::stopsGeoJson(stops, 0));
    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    QCOMPARE(features.size(), 2);
    QCOMPARE(features.at(1).toObject().value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("label")).toString(), QStringLiteral("B"));
}

void MapPlanGeometryTest::emptyStopsIsAnEmptyCollection()
{
    const QJsonObject root = parse(MapPlanGeometry::stopsGeoJson({}, 0));
    QCOMPARE(root.value(QStringLiteral("type")).toString(), QStringLiteral("FeatureCollection"));
    QVERIFY(root.value(QStringLiteral("features")).toArray().isEmpty());
}

QTEST_MAIN(MapPlanGeometryTest)
#include "MapPlanGeometryTest.moc"
