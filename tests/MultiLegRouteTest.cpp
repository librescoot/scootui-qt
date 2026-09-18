#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "routing/RouteHelpers.h"

using namespace RouteHelpers;

// Multi-leg parsing for the plan overview. Fixtures are synthetic (a hand-built
// Valhalla trip object), so the test does not depend on a captured response.
class MultiLegRouteTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesEachLegIndependently();
    void mergedShapeDropsTheDuplicateJoint();
    void singleLegParserStillTakesTheFirstLeg();
    void reportsFailureForNullOrEmptyLegs();
    void skipsLegsWithoutAShape();
};

namespace {

// leg 0: (52.5200,13.4510) -> (52.5180,13.4515) -> (52.5160,13.4520)
const char *kLeg0Shape = "_cqdcBon~sX~{Bg^~{Bg^";
// leg 1 shares the joint then runs to (52.5259,13.3662)
const char *kLeg1Shape = "_iidcB_m`tX_yF~_qAwoJnptA";

QJsonObject makeLeg(const QString &shape, double lengthKm, double timeSeconds,
                    const QJsonArray &maneuvers)
{
    return QJsonObject{
        {QStringLiteral("summary"),
         QJsonObject{{QStringLiteral("length"), lengthKm},
                     {QStringLiteral("time"), timeSeconds}}},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("maneuvers"), maneuvers}};
}

QJsonArray makeManeuvers()
{
    return QJsonArray{
        QJsonObject{{QStringLiteral("type"), 1},
                    {QStringLiteral("begin_shape_index"), 0},
                    {QStringLiteral("length"), 0.2},
                    {QStringLiteral("time"), 30.0},
                    {QStringLiteral("instruction"), QStringLiteral("Drive")}},
        QJsonObject{{QStringLiteral("type"), 5},
                    {QStringLiteral("begin_shape_index"), 2},
                    {QStringLiteral("length"), 0.0},
                    {QStringLiteral("time"), 0.0},
                    {QStringLiteral("instruction"), QStringLiteral("Arrive")}}};
}

QByteArray makeTrip(bool withSecondLeg = true, bool secondLegHasShape = true)
{
    QJsonArray legs;
    legs.append(makeLeg(QString::fromLatin1(kLeg0Shape), 0.628, 103.8, makeManeuvers()));
    if (withSecondLeg) {
        legs.append(makeLeg(secondLegHasShape ? QString::fromLatin1(kLeg1Shape) : QString(),
                            6.987, 1005.8, makeManeuvers()));
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("trip"), QJsonObject{{QStringLiteral("legs"), legs}}}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

void MultiLegRouteTest::parsesEachLegIndependently()
{
    QList<Route> legs;
    QVERIFY(parseMultiLegRouteResponse(makeTrip(), legs));
    QCOMPARE(legs.size(), 2);

    QCOMPARE(legs[0].waypoints.size(), 3);
    QCOMPARE(legs[1].waypoints.size(), 3);
    QCOMPARE(legs[0].distance, 628.0);
    QCOMPARE(legs[0].duration, 103.8);
    QCOMPARE(legs[1].distance, 6987.0);
    QCOMPARE(legs[1].duration, 1005.8);

    // Shape indices stay relative to their own leg.
    QCOMPARE(legs[0].instructions.first().originalShapeIndex, 0);
    QCOMPARE(legs[0].instructions.last().type, ManeuverType::ArriveRight);
    QCOMPARE(legs[1].instructions.first().type, ManeuverType::KeepStraight);
    QCOMPARE(legs[0].waypoints.first().latitude, 52.52);
    QCOMPARE(legs[1].waypoints.last().latitude, 52.5259);
}

void MultiLegRouteTest::mergedShapeDropsTheDuplicateJoint()
{
    QList<Route> legs;
    QList<LatLng> merged;
    QVERIFY(parseMultiLegRouteResponse(makeTrip(), legs, &merged));

    // 3 + 3 points minus the shared joint vertex.
    QCOMPARE(merged.size(), 5);
    QCOMPARE(merged[2], legs[0].waypoints.last());
    QCOMPARE(merged[2], legs[1].waypoints.first());
    QCOMPARE(merged[3], legs[1].waypoints[1]);
}

void MultiLegRouteTest::singleLegParserStillTakesTheFirstLeg()
{
    const Route route = parseRouteResponse(makeTrip());
    QVERIFY(route.isValid());
    QCOMPARE(route.waypoints.size(), 3);
    QCOMPARE(route.distance, 628.0);
    QCOMPARE(route.waypoints.last().latitude, 52.5160);
}

void MultiLegRouteTest::reportsFailureForNullOrEmptyLegs()
{
    QList<Route> legs;
    QVERIFY(!parseMultiLegRouteResponse(QByteArray("not json"), legs));
    QVERIFY(legs.isEmpty());

    const QByteArray noLegs = QJsonDocument(QJsonObject{
        {QStringLiteral("trip"), QJsonObject{{QStringLiteral("legs"), QJsonArray{}}}}})
        .toJson(QJsonDocument::Compact);
    QVERIFY(!parseMultiLegRouteResponse(noLegs, legs));
    QVERIFY(legs.isEmpty());

    QVERIFY(!parseRouteResponse(noLegs).isValid());
}

void MultiLegRouteTest::skipsLegsWithoutAShape()
{
    QList<Route> legs;
    QVERIFY(parseMultiLegRouteResponse(makeTrip(true, false), legs));
    QCOMPARE(legs.size(), 1);
    QCOMPARE(legs[0].distance, 628.0);
}

QTEST_MAIN(MultiLegRouteTest)
#include "MultiLegRouteTest.moc"
