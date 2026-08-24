#include <QtTest>

#include "services/MapCameraPolicy.h"
#include "services/NavigationCadence.h"

class MapTimingTest : public QObject
{
    Q_OBJECT

private slots:
    void dividersStayOnIntegerRenderTicks();
    void overviewFitsRouteExtent();
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
    QVERIFY(MapCameraPolicy::routeOverviewZoom(longRoute) < 14.0);
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
