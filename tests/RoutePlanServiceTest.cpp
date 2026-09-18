#include <QtTest>

#include "core/AppConfig.h"
#include "repositories/InMemoryMdbRepository.h"
#include "services/RoutePlanService.h"

// Durable-plan and wire-format tests. Redis is the only copy on a running
// vehicle (no AOF), so the settings-hash round trip and the JSON parse have to
// be exact before anything is built on top of them.
class RoutePlanServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void saveThenLoadRoundTripsStopsAndStep();
    void saveShorterPlanDropsStaleRecords();
    void clearRemovesEveryRecord();
    void loadClampsOutOfRangeStep();
    void loadSkipsMalformedCoordinates();
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

static QString settingsKey(const QString &suffix)
{
    return QStringLiteral("%1.%2")
        .arg(QString::fromLatin1(AppConfig::routePlanPrefix))
        .arg(suffix);
}

void RoutePlanServiceTest::saveThenLoadRoundTripsStopsAndStep()
{
    InMemoryMdbRepository repo;
    RoutePlanService service(&repo);

    RoutePlan plan;
    plan.stops = {stop(52.51, 13.30, QStringLiteral("Home")),
                  stop(52.52, 13.40, QStringLiteral("Work")),
                  stop(52.53, 13.50, QStringLiteral("Gym"))};
    plan.currentStep = 1;
    plan.stops[0].reached = true;
    QVERIFY(service.save(plan));

    const RoutePlan loaded = service.load();
    QCOMPARE(loaded.stopCount(), 3);
    QCOMPARE(loaded.currentStep, 1);
    QCOMPARE(loaded.stops[0].label, QStringLiteral("Home"));
    QVERIFY(loaded.stops[0].reached);
    QCOMPARE(loaded.stops[1].position.latitude, 52.52);
    QCOMPARE(loaded.stops[2].label, QStringLiteral("Gym"));
    // Ids are assigned in list order and are not persisted.
    QCOMPARE(loaded.stops[0].id, 1);
    QCOMPARE(loaded.stops[2].id, 3);
}

void RoutePlanServiceTest::saveShorterPlanDropsStaleRecords()
{
    InMemoryMdbRepository repo;
    RoutePlanService service(&repo);

    RoutePlan plan;
    plan.stops = {stop(52.51, 13.30), stop(52.52, 13.40), stop(52.53, 13.50)};
    QVERIFY(service.save(plan));
    QVERIFY(!repo.get(QStringLiteral("settings"), settingsKey(QStringLiteral("2.latitude"))).isEmpty());

    RoutePlan shorter;
    shorter.stops = {stop(52.51, 13.30)};
    QVERIFY(service.save(shorter));

    QVERIFY(repo.get(QStringLiteral("settings"), settingsKey(QStringLiteral("1.latitude"))).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"), settingsKey(QStringLiteral("2.latitude"))).isEmpty());
    QCOMPARE(service.load().stopCount(), 1);
}

void RoutePlanServiceTest::clearRemovesEveryRecord()
{
    InMemoryMdbRepository repo;
    RoutePlanService service(&repo);

    RoutePlan plan;
    plan.stops = {stop(52.51, 13.30), stop(52.52, 13.40)};
    plan.currentStep = 1;
    QVERIFY(service.save(plan));
    QVERIFY(service.clear());

    QVERIFY(repo.get(QStringLiteral("settings"), settingsKey(QStringLiteral("0.latitude"))).isEmpty());
    QVERIFY(repo.get(QStringLiteral("settings"), settingsKey(QStringLiteral("current-step"))).isEmpty());
    QVERIFY(!service.load().isValid());
}

void RoutePlanServiceTest::loadClampsOutOfRangeStep()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("0.latitude")), QStringLiteral("52.51"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("0.longitude")), QStringLiteral("13.30"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("1.latitude")), QStringLiteral("52.52"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("1.longitude")), QStringLiteral("13.40"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("current-step")), QStringLiteral("9"));

    RoutePlanService service(&repo);
    const RoutePlan loaded = service.load();
    QCOMPARE(loaded.stopCount(), 2);
    QCOMPARE(loaded.currentStep, 1);
}

void RoutePlanServiceTest::loadSkipsMalformedCoordinates()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("0.latitude")), QStringLiteral("not-a-number"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("0.longitude")), QStringLiteral("13.30"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("1.latitude")), QStringLiteral("0"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("1.longitude")), QStringLiteral("0"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("2.latitude")), QStringLiteral("52.52"));
    repo.set(QStringLiteral("settings"), settingsKey(QStringLiteral("2.longitude")), QStringLiteral("13.40"));

    RoutePlanService service(&repo);
    const RoutePlan loaded = service.load();
    QCOMPARE(loaded.stopCount(), 1);
    QCOMPARE(loaded.stops[0].position.latitude, 52.52);
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
