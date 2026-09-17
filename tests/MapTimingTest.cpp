#include <QtTest>

#include "services/MapCameraPolicy.h"
#include "services/NavigationCadence.h"

class MapTimingTest : public QObject
{
    Q_OBJECT

private slots:
    void dividersStayOnIntegerRenderTicks();
    void overviewFitsRouteExtent();
    void overviewZoomCoversScooterRange();
    void overviewCentersBounds();
    void closeSecondTurnExpandsLookahead();
};

void MapTimingTest::dividersStayOnIntegerRenderTicks()
{
    NavigationCadence::TickDivider navigation(
        NavigationCadence::NavigationEveryTicks);
    NavigationCadence::TickDivider roadInfo(
        NavigationCadence::RoadInfoEveryTicks);
    int navigationUpdates = 0;
    int roadUpdates = 0;
    for (int tick = 1; tick <= 100; ++tick) {
        navigationUpdates += navigation.advance() ? 1 : 0;
        roadUpdates += roadInfo.advance() ? 1 : 0;
    }
    QCOMPARE(navigationUpdates, 25);
    QCOMPARE(roadUpdates, 5);
}

void MapTimingTest::overviewFitsRouteExtent()
{
    const QList<LatLng> shortRoute{{52.5, 13.4}, {52.501, 13.4}};
    const QList<LatLng> longRoute{{52.5, 13.4}, {52.6, 13.4}};
    QCOMPARE(MapCameraPolicy::routeOverviewZoom(shortRoute), 15.0);
    const double longZoom = MapCameraPolicy::routeOverviewZoom(longRoute);
    QVERIFY(longZoom > 11.0);
    QVERIFY(longZoom < 12.0);
}

// A full-range (45 km) route must be framed without hitting the floor.
void MapTimingTest::overviewZoomCoversScooterRange()
{
    // 0.405 degrees of latitude is about 45 km.
    const QList<LatLng> rangeRoute{{52.5, 13.4}, {52.905, 13.4}};
    const double rangeZoom = MapCameraPolicy::routeOverviewZoom(rangeRoute);
    QVERIFY(rangeZoom < 11.0);
    QVERIFY(rangeZoom >= 9.0);

    // Longer routes stop at the floor instead of zooming out forever.
    const QList<LatLng> countryRoute{{52.0, 13.4}, {53.5, 13.4}};
    QCOMPARE(MapCameraPolicy::routeOverviewZoom(countryRoute), 9.0);
}

void MapTimingTest::overviewCentersBounds()
{
    const QList<LatLng> route{{52.5, 13.4}, {52.6, 13.7}, {52.55, 13.5}};
    const LatLng center = MapCameraPolicy::routeOverviewCenter(route);
    QCOMPARE(center.latitude, 52.55);
    QCOMPARE(center.longitude, 13.55);

    const LatLng empty = MapCameraPolicy::routeOverviewCenter({});
    QVERIFY(!empty.isValid());
    const LatLng single = MapCameraPolicy::routeOverviewCenter({{52.5, 13.4}});
    QCOMPARE(single.latitude, 52.5);
    QCOMPARE(single.longitude, 13.4);
}

void MapTimingTest::closeSecondTurnExpandsLookahead()
{
    QCOMPARE(MapCameraPolicy::maneuverFocusDistance(40.0, 130.0, 150.0),
             130.0);
    QCOMPARE(MapCameraPolicy::maneuverFocusDistance(40.0, 200.0, 150.0),
             40.0);
}

QTEST_APPLESS_MAIN(MapTimingTest)
#include "MapTimingTest.moc"
