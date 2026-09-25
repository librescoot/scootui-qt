#include <QtTest>

#include "services/RoutePlanService.h"

// Compatibility waypoint serialization and parsing tests.
class RoutePlanServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void serializeParseRoundTrip();
    void parseRejectsMalformedPayloads();
    void parseAcceptsStringCoordinates();
    void parseAcceptsCloudFieldNames();
};

static RouteStop stop(double latitude, double longitude, const QString &label = {})
{
    RouteStop result;
    result.position = {latitude, longitude};
    result.label = label;
    return result;
}

void RoutePlanServiceTest::serializeParseRoundTrip()
{
    QList<RouteStop> stops = {stop(52.51, 13.30, QStringLiteral("Home")),
                              stop(52.52, 13.40)};
    const QString json = RoutePlanService::serializeWaypoints(stops);
    QVERIFY(!json.isEmpty());

    bool ok = false;
    const QList<RouteStop> parsed = RoutePlanService::parseWaypoints(json, &ok);
    QVERIFY(ok);
    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed[0].position.latitude, 52.51);
    QCOMPARE(parsed[0].label, QStringLiteral("Home"));
    QVERIFY(parsed[1].label.isEmpty());
    QCOMPARE(parsed[0].id, 1);
    QCOMPARE(parsed[1].id, 2);
}

void RoutePlanServiceTest::parseRejectsMalformedPayloads()
{
    bool ok = true;
    QVERIFY(RoutePlanService::parseWaypoints(QString(), &ok).isEmpty());
    QVERIFY(!ok);

    ok = true;
    QVERIFY(RoutePlanService::parseWaypoints(QStringLiteral("not json"), &ok).isEmpty());
    QVERIFY(!ok);

    ok = true;
    QVERIFY(RoutePlanService::parseWaypoints(QStringLiteral("{\"lat\":1}"), &ok).isEmpty());
    QVERIFY(!ok);

    // Entries without valid coordinates are skipped, not fatal.
    ok = false;
    const auto partial = RoutePlanService::parseWaypoints(
        QStringLiteral("[{\"lat\":0,\"lon\":0},{\"lat\":52.5,\"lon\":13.4}]"), &ok);
    QVERIFY(ok);
    QCOMPARE(partial.size(), 1);
    QCOMPARE(partial[0].position.latitude, 52.5);
}

void RoutePlanServiceTest::parseAcceptsStringCoordinates()
{
    bool ok = false;
    const auto parsed = RoutePlanService::parseWaypoints(
        QStringLiteral("[{\"lat\":\"52.51\",\"lon\":\"13.30\",\"label\":\"A\"}]"), &ok);
    QVERIFY(ok);
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed[0].position.latitude, 52.51);
    QCOMPARE(parsed[0].label, QStringLiteral("A"));
}

void RoutePlanServiceTest::parseAcceptsCloudFieldNames()
{
    bool ok = false;
    const auto parsed = RoutePlanService::parseWaypoints(
        QStringLiteral("[{\"latitude\":52.51,\"longitude\":13.30,\"name\":\"Home\"}]"), &ok);
    QVERIFY(ok);
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed[0].position.latitude, 52.51);
    QCOMPARE(parsed[0].position.longitude, 13.30);
    QCOMPARE(parsed[0].label, QStringLiteral("Home"));
}

QTEST_MAIN(RoutePlanServiceTest)
#include "RoutePlanServiceTest.moc"
