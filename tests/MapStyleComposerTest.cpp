#include <QtTest>

#include "services/MapStyleComposer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const QByteArray Style = R"STYLE({
  "version": 8,
  "sprite": "example-sprite",
  "glyphs": "example-glyphs/{fontstack}/{range}.pbf",
  "metadata": { "owner": "test" },
  "sources": {
    "basemap": { "type": "vector", "tiles": ["example-tiles/{z}/{x}/{y}"] },
    "google-traffic": { "type": "raster", "tiles": ["example-traffic/{z}/{x}/{y}"] }
  },
  "layers": [
    { "id": "background", "type": "background" },
    { "id": "traffic-overlay", "type": "raster", "source": "google-traffic" },
    { "id": "buildings", "type": "fill-extrusion", "source": "basemap",
      "paint": { "fill-extrusion-color": "#123456", "fill-extrusion-opacity": 0.4,
                 "fill-extrusion-height": ["get", "height"],
                 "fill-extrusion-pattern": "brick",
                 "fill-extrusion-translate": [2, 3],
                 "fill-extrusion-translate-anchor": "viewport" } },
    { "id": "building-compat", "type": "fill-extrusion", "source": "basemap",
      "paint": { "fill-extrusion-color": "#654321", "fill-extrusion-opacity": 0.5 } },
    { "id": "labels", "type": "symbol", "source": "basemap" }
  ]
})STYLE";

QJsonObject root(const MapStyleComposeResult &result)
{
    return QJsonDocument::fromJson(result.json).object();
}

QJsonObject layer(const QJsonObject &style, const QString &id)
{
    for (const QJsonValue &value : style.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value(QStringLiteral("id")).toString() == id)
            return candidate;
    }
    return {};
}

int layerIndex(const QJsonObject &style, const QString &id)
{
    const QJsonArray layers = style.value(QStringLiteral("layers")).toArray();
    for (int i = 0; i < layers.size(); ++i) {
        if (layers.at(i).toObject().value(QStringLiteral("id")).toString() == id)
            return i;
    }
    return -1;
}

MapStyleConfig config()
{
    MapStyleConfig value;
    value.revision = QStringLiteral("map:42");
    value.route.fillColor = QStringLiteral("#abcdef");
    value.route.borderColor = QStringLiteral("#102030");
    value.route.fillWidth = 8;
    value.route.borderWidth = 12;
    return value;
}

} // namespace

class MapStyleComposerTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesOnlineResourcesAndPlacesRoute();
    void configuresOfflineResourcesWithoutGlyphs();
    void keepsOfflineLabelsWhenGlyphsExist();
    void flattensTwoDimensionalStyleAndRemovesTraffic();
    void rejectsInvalidJson();
    void compositionIsIdempotent();
};

void MapStyleComposerTest::preservesOnlineResourcesAndPlacesRoute()
{
    const MapStyleComposeResult result = composeMapStyle(Style, config());
    QVERIFY2(result, qPrintable(result.error));
    const QJsonObject style = root(result);

    const QJsonObject basemap = style.value(QStringLiteral("sources")).toObject()
                                      .value(QStringLiteral("basemap")).toObject();
    QVERIFY(basemap.contains(QStringLiteral("tiles")));
    QVERIFY(style.contains(QStringLiteral("sprite")));
    QVERIFY(style.contains(QStringLiteral("glyphs")));
    QCOMPARE(style.value(QStringLiteral("metadata")).toObject()
                 .value(QStringLiteral("owner")).toString(), QStringLiteral("test"));
    QCOMPARE(style.value(QStringLiteral("metadata")).toObject()
                 .value(QStringLiteral("scootui-style-revision")).toString(),
             QStringLiteral("map:42"));

    QVERIFY(layerIndex(style, QStringLiteral("route-border"))
            < layerIndex(style, QStringLiteral("buildings")));
    QVERIFY(layerIndex(style, QStringLiteral("route-fill"))
            < layerIndex(style, QStringLiteral("buildings")));
    QVERIFY(layerIndex(style, QStringLiteral("route-ghost"))
            > layerIndex(style, QStringLiteral("building-compat")));
    QVERIFY(layerIndex(style, QStringLiteral("route-ghost"))
            < layerIndex(style, QStringLiteral("labels")));
    QCOMPARE(layer(style, QStringLiteral("route-fill")).value(QStringLiteral("paint")).toObject()
                 .value(QStringLiteral("line-color")).toString(), QStringLiteral("#abcdef"));
    QCOMPARE(layer(style, QStringLiteral("route-border")).value(QStringLiteral("paint")).toObject()
                 .value(QStringLiteral("line-width")).toInt(), 12);
}

void MapStyleComposerTest::configuresOfflineResourcesWithoutGlyphs()
{
    MapStyleConfig value = config();
    value.mbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
    const QJsonObject style = root(composeMapStyle(Style, value));
    const QJsonObject basemap = style.value(QStringLiteral("sources")).toObject()
                                      .value(QStringLiteral("basemap")).toObject();

    QCOMPARE(basemap.value(QStringLiteral("url")).toString(),
             QStringLiteral("mbtiles:///data/maps/map.mbtiles"));
    QVERIFY(!basemap.contains(QStringLiteral("tiles")));
    QCOMPARE(basemap.value(QStringLiteral("maxzoom")).toInt(), 14);
    QVERIFY(!style.contains(QStringLiteral("sprite")));
    QVERIFY(!style.contains(QStringLiteral("glyphs")));
    QVERIFY(layer(style, QStringLiteral("labels")).isEmpty());
    QVERIFY(!layer(style, QStringLiteral("traffic-overlay")).isEmpty());
}

void MapStyleComposerTest::keepsOfflineLabelsWhenGlyphsExist()
{
    MapStyleConfig value = config();
    value.mbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
    value.glyphDirectory = QStringLiteral("/usr/share/scootui/glyphs");
    const QJsonObject style = root(composeMapStyle(Style, value));

    QCOMPARE(style.value(QStringLiteral("glyphs")).toString(),
             QStringLiteral("file:///usr/share/scootui/glyphs/{fontstack}/{range}.pbf"));
    QVERIFY(!layer(style, QStringLiteral("labels")).isEmpty());
}

void MapStyleComposerTest::flattensTwoDimensionalStyleAndRemovesTraffic()
{
    MapStyleConfig value = config();
    value.view2D = true;
    value.trafficVisible = false;
    const QJsonObject style = root(composeMapStyle(Style, value));

    QVERIFY(!style.value(QStringLiteral("sources")).toObject()
                 .contains(QStringLiteral("google-traffic")));
    QVERIFY(layer(style, QStringLiteral("traffic-overlay")).isEmpty());
    QCOMPARE(layer(style, QStringLiteral("buildings")).value(QStringLiteral("type")).toString(),
             QStringLiteral("fill"));
    const QJsonObject paint = layer(style, QStringLiteral("buildings"))
                                  .value(QStringLiteral("paint")).toObject();
    QCOMPARE(paint.value(QStringLiteral("fill-color")).toString(), QStringLiteral("#123456"));
    QCOMPARE(paint.value(QStringLiteral("fill-opacity")).toDouble(), 0.4);
    QCOMPARE(paint.value(QStringLiteral("fill-pattern")).toString(), QStringLiteral("brick"));
    QCOMPARE(paint.value(QStringLiteral("fill-translate")).toArray(), QJsonArray({2, 3}));
    QCOMPARE(paint.value(QStringLiteral("fill-translate-anchor")).toString(),
             QStringLiteral("viewport"));
    QVERIFY(!paint.contains(QStringLiteral("fill-extrusion-height")));
    QVERIFY(layer(style, QStringLiteral("route-ghost")).isEmpty());
    QVERIFY(layerIndex(style, QStringLiteral("route-fill"))
            < layerIndex(style, QStringLiteral("labels")));
}

void MapStyleComposerTest::rejectsInvalidJson()
{
    const MapStyleComposeResult result = composeMapStyle("not json", config());
    QVERIFY(!result);
    QVERIFY(result.json.isEmpty());
    QVERIFY(!result.error.isEmpty());
}

void MapStyleComposerTest::compositionIsIdempotent()
{
    const MapStyleComposeResult first = composeMapStyle(Style, config());
    QVERIFY(first);
    const MapStyleComposeResult second = composeMapStyle(first.json, config());
    QVERIFY(second);
    const QJsonObject style = root(second);

    int routeLayers = 0;
    for (const QJsonValue &value : style.value(QStringLiteral("layers")).toArray()) {
        if (value.toObject().value(QStringLiteral("id")).toString().startsWith(QStringLiteral("route-")))
            ++routeLayers;
    }
    QCOMPARE(routeLayers, 3);
}

QTEST_APPLESS_MAIN(MapStyleComposerTest)
#include "MapStyleComposerTest.moc"
