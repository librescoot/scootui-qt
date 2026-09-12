#include <QtTest>

#include "services/MapStyleMetadata.h"

class MapStyleMetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenMetadataMissing();
    void readsRouteKeys();
    void ignoresMalformedValues();
    void partialOverrideKeepsOtherDefaults();
};

void MapStyleMetadataTest::defaultsWhenMetadataMissing()
{
    const MapRouteStyle style = parseMapRouteStyle(R"({"id":"dark"})");

    QCOMPARE(style.fillColor, QStringLiteral("#42A5F5"));
    QCOMPARE(style.borderColor, QStringLiteral("#1565C0"));
    QCOMPARE(style.fillWidth, 7);
    QCOMPARE(style.borderWidth, 11);
}

void MapStyleMetadataTest::readsRouteKeys()
{
    const MapRouteStyle style = parseMapRouteStyle(R"({
        "metadata": {
            "route-fill-color": "#112233",
            "route-border-color": "#445566",
            "route-fill-width": 3,
            "route-border-width": 5
        }
    })");

    QCOMPARE(style.fillColor, QStringLiteral("#112233"));
    QCOMPARE(style.borderColor, QStringLiteral("#445566"));
    QCOMPARE(style.fillWidth, 3);
    QCOMPARE(style.borderWidth, 5);
}

void MapStyleMetadataTest::ignoresMalformedValues()
{
    const MapRouteStyle style = parseMapRouteStyle(R"({
        "metadata": {
            "route-fill-color": "",
            "route-border-color": 42,
            "route-fill-width": 0,
            "route-border-width": -3
        }
    })");

    QCOMPARE(style.fillColor, QStringLiteral("#42A5F5"));
    QCOMPARE(style.borderColor, QStringLiteral("#1565C0"));
    QCOMPARE(style.fillWidth, 7);
    QCOMPARE(style.borderWidth, 11);
}

void MapStyleMetadataTest::partialOverrideKeepsOtherDefaults()
{
    const MapRouteStyle style = parseMapRouteStyle(
        R"({"metadata": {"route-fill-color": "#010203"}})");

    QCOMPARE(style.fillColor, QStringLiteral("#010203"));
    QCOMPARE(style.borderColor, QStringLiteral("#1565C0"));
    QCOMPARE(style.fillWidth, 7);
    QCOMPARE(style.borderWidth, 11);
}

QTEST_APPLESS_MAIN(MapStyleMetadataTest)
#include "MapStyleMetadataTest.moc"
