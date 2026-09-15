#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>

#include "stores/EngineStore.h"
#include "stores/TripStore.h"

#include "repositories/InMemoryMdbRepository.h"
#include "stores/VehicleStore.h"

class RecordingRepository : public InMemoryMdbRepository
{
public:
    void push(const QString &channel, const QString &command) override
    {
        pushed << qMakePair(channel, command);
    }

    QList<QPair<QString, QString>> pushed;
};

class SyncableStoreSeedTest : public QObject
{
    Q_OBJECT

private slots:
    void startSeedsFromExistingCache();
    void startWithEmptyCacheKeepsDefaults();
    void inMemoryReportsSeeded();
    void blinkerUsesSnapshotAnchorOnStartupAndRestart();
    void blinkerWaitsForAnchor();
    void tripCounterHydratesWithDisplayUnits();
    void tripCounterAbsentApiVersionKeepsRideFallback();
    void tripCounterCorrelatesOnlyItsResetResult();
    void tripCounterTimeoutRetriesWithSameIdUntilTerminalResult();
    void tripCounterSuccessfulAcknowledgementAllowsRetry();
    void tripCounterApiAppearanceAndDisappearanceRecoversFallback();
    void tripCounterLeaseExpiryDisablesReadsAndResets();
    void tripCounterAcceptsZeroAndLargeSnapshots();
    void tripCounterSimulatorOverrideStaysSeparate();
};

void SyncableStoreSeedTest::startSeedsFromExistingCache()
{
    InMemoryMdbRepository repo;
    repo.set(QStringLiteral("vehicle"), QStringLiteral("state"),
             QStringLiteral("ready-to-drive"), false);

    VehicleStore store(&repo);
    QCOMPARE(store.stateRaw(), QString());
    store.start();
    QCOMPARE(store.stateRaw(), QStringLiteral("ready-to-drive"));
}

void SyncableStoreSeedTest::startWithEmptyCacheKeepsDefaults()
{
    InMemoryMdbRepository repo;
    VehicleStore store(&repo);
    store.start();
    QCOMPARE(store.stateRaw(), QString());
}

void SyncableStoreSeedTest::inMemoryReportsSeeded()
{
    InMemoryMdbRepository repo;
    QVERIFY(repo.isDataSeeded());
}

void SyncableStoreSeedTest::blinkerUsesSnapshotAnchorOnStartupAndRestart()
{
    InMemoryMdbRepository repo;
    const auto anchor = [](int phase) {
        return QString::number((QDateTime::currentMSecsSinceEpoch() - phase) * 1000000LL);
    };
    repo.set("vehicle", "blinker:state", "right", false);
    repo.set("vehicle", "blinker:start_nanos", anchor(250), false);
    VehicleStore store(&repo);
    store.start();
    QVERIFY(store.blinkOpacity() > 0.9);

    // Same direction, different hardware cycle: no state-change signal needed.
    QSignalSpy states(&store, &VehicleStore::blinkerStateChanged);
    repo.set("vehicle", "blinker:start_nanos", anchor(650), false);
    repo.requestAll("vehicle");
    QCOMPARE(store.blinkOpacity(), 0.0);
    QCOMPARE(states.count(), 0);
    repo.set("vehicle", "blinker:start_nanos", anchor(250), false);
    repo.requestAll("vehicle");
    QVERIFY(store.blinkOpacity() > 0.9);
    QCOMPARE(states.count(), 0);

    repo.set("vehicle", "blinker:state", "off");
    QCOMPARE(store.blinkOpacity(), 0.0);
}

void SyncableStoreSeedTest::blinkerWaitsForAnchor()
{
    InMemoryMdbRepository repo;
    repo.set("vehicle", "blinker:state", "right", false);
    VehicleStore store(&repo);
    store.start();
    QTest::qWait(100);
    QCOMPARE(store.blinkOpacity(), 0.0);
}


void SyncableStoreSeedTest::tripCounterHydratesWithDisplayUnits()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    repo.set("trip:counter", "distance-m", "12345", false);
    repo.set("trip:counter", "duration-s", "678", false);
    repo.set("trip:counter", "average-speed-kmh", "66", false);
    repo.set("trip:counter", "reset-policy", "day", false);
    repo.set("trip:counter", "reset-at", "1700000000", false);
    repo.set("trip:counter", "reset-reason", "day", false);
    repo.set("trip:counter", "generation", "7", false);
    repo.set("trip:counter", "status", "recording", false);
    repo.set("trip:counter", "updated-at", "1700000001", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    QVERIFY(trip.persistentAvailable());
    QCOMPARE(trip.distance(), 12.345);
    QCOMPARE(trip.duration(), 678);
    QCOMPARE(trip.averageSpeed(), 66.0);
    QCOMPARE(trip.resetPolicy(), QStringLiteral("day"));
    QCOMPARE(trip.generation(), 7);
    QCOMPARE(trip.status(), QStringLiteral("recording"));
}

void SyncableStoreSeedTest::tripCounterAbsentApiVersionKeepsRideFallback()
{
    RecordingRepository repo;
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    QVERIFY(!trip.persistentAvailable());
    repo.set("engine-ecu", "speed", "36");
    repo.set("vehicle", "state", "ready-to-drive");
    QTest::qWait(2100);
    QVERIFY(trip.distance() > 0.005);
    QVERIFY(trip.duration() >= 1);
    trip.reset();
    QCOMPARE(trip.duration(), 0);
    QVERIFY(repo.pushed.isEmpty());
}

void SyncableStoreSeedTest::tripCounterCorrelatesOnlyItsResetResult()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    trip.reset();
    QCOMPARE(trip.resetState(), QStringLiteral("pending"));
    QCOMPARE(repo.pushed.size(), 1);
    QCOMPARE(repo.pushed.first().first, QStringLiteral("scooter:trip"));
    const QJsonObject command = QJsonDocument::fromJson(repo.pushed.first().second.toUtf8()).object();
    QCOMPARE(command.value("op").toString(), QStringLiteral("counter.reset"));
    QCOMPARE(command.value("source").toString(), QStringLiteral("scootui"));
    trip.reset();
    QCOMPARE(repo.pushed.size(), 1);

    repo.publish("trip:command-result", R"({"id":"other","op":"counter.reset","status":"ok","error":""})");
    QCOMPARE(trip.resetState(), QStringLiteral("pending"));
    repo.publish("trip:command-result", QStringLiteral(R"({"id":"%1","op":"counter.reset","status":"error","error":"busy"})")
                 .arg(command.value("id").toString()));
    QCOMPARE(trip.resetState(), QStringLiteral("error"));
    QCOMPARE(trip.resetError(), QStringLiteral("busy"));
}

void SyncableStoreSeedTest::tripCounterTimeoutRetriesWithSameIdUntilTerminalResult()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    trip.reset();
    QCOMPARE(repo.pushed.size(), 1);
    const QJsonObject first = QJsonDocument::fromJson(repo.pushed.first().second.toUtf8()).object();
    const QString firstId = first.value("id").toString();
    QTRY_COMPARE_WITH_TIMEOUT(trip.resetState(), QStringLiteral("error"), 6000);
    QCOMPARE(trip.resetError(), QStringLiteral("Trip service unavailable"));

    trip.reset();
    QCOMPARE(repo.pushed.size(), 2);
    const QJsonObject retry = QJsonDocument::fromJson(repo.pushed.last().second.toUtf8()).object();
    QCOMPARE(retry.value("id").toString(), firstId);

    repo.publish("trip:command-result", QStringLiteral(R"({"id":"%1","op":"counter.reset","status":"ok","error":""})")
                 .arg(firstId));
    QCOMPARE(trip.resetState(), QStringLiteral("success"));
    trip.reset();
    QCOMPARE(repo.pushed.size(), 3);
    const QJsonObject next = QJsonDocument::fromJson(repo.pushed.last().second.toUtf8()).object();
    QVERIFY(next.value("id").toString() != firstId);
}

void SyncableStoreSeedTest::tripCounterSuccessfulAcknowledgementAllowsRetry()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    trip.reset();
    const QJsonObject first = QJsonDocument::fromJson(repo.pushed.first().second.toUtf8()).object();
    repo.publish("trip:command-result", QStringLiteral(R"({"id":"%1","op":"counter.reset","status":"ok","error":""})")
                 .arg(first.value("id").toString()));
    QCOMPARE(trip.resetState(), QStringLiteral("success"));
    trip.reset();
    QCOMPARE(repo.pushed.size(), 2);
    const QJsonObject second = QJsonDocument::fromJson(repo.pushed.last().second.toUtf8()).object();
    QVERIFY(second.value("id").toString() != first.value("id").toString());
}

void SyncableStoreSeedTest::tripCounterApiAppearanceAndDisappearanceRecoversFallback()
{
    RecordingRepository repo;
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    engine.start();
    vehicle.start();
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();
    repo.set("engine-ecu", "speed", "36");
    repo.set("vehicle", "state", "ready-to-drive");

    repo.set("trip:counter", "distance-m", "2000");
    repo.set("trip:counter", "duration-s", "73");
    QVERIFY(!trip.persistentAvailable());
    QVERIFY(trip.duration() == 0);

    repo.set("trip:counter", "api-version", "1");
    QVERIFY(trip.persistentAvailable());
    QCOMPARE(trip.distance(), 2.0);
    QCOMPARE(trip.duration(), 73);

    repo.hdel("trip:counter", "api-version");
    QTRY_VERIFY(!trip.persistentAvailable());
    QCOMPARE(trip.duration(), 0);
    QTest::qWait(2100);
    QVERIFY(trip.distance() > 0.005);
}

void SyncableStoreSeedTest::tripCounterLeaseExpiryDisablesReadsAndResets()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    repo.set("trip:counter", "distance-m", "2000", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();
    QVERIFY(trip.persistentAvailable());

    repo.setValue(QStringLiteral("trip:ready"), QString());
    QVERIFY(!trip.persistentAvailable());
    trip.reset();
    QCOMPARE(trip.resetState(), QStringLiteral("error"));
    QVERIFY(repo.pushed.isEmpty());

    repo.setValue(QStringLiteral("trip:ready"), QStringLiteral("1"));
    QVERIFY(trip.persistentAvailable());
    trip.reset();
    QCOMPARE(repo.pushed.size(), 1);
    const QJsonObject request = QJsonDocument::fromJson(repo.pushed.constLast().second.toUtf8()).object();
    const qint64 expiresAt = request.value(QStringLiteral("expires-at")).toInteger();
    QVERIFY(expiresAt > QDateTime::currentMSecsSinceEpoch());
    QVERIFY(expiresAt <= QDateTime::currentMSecsSinceEpoch() + 5100);

    repo.setValue(QStringLiteral("trip:ready"), QString());
    QVERIFY(!trip.persistentAvailable());
    QCOMPARE(trip.resetState(), QStringLiteral("error"));
    QCOMPARE(repo.pushed.size(), 1);
}

void SyncableStoreSeedTest::tripCounterAcceptsZeroAndLargeSnapshots()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    repo.set("trip:counter", "distance-m", "0", false);
    repo.set("trip:counter", "duration-s", "0", false);
    repo.set("trip:counter", "average-speed-kmh", "0", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();
    QCOMPARE(trip.distance(), 0.0);
    QCOMPARE(trip.duration(), 0);
    QCOMPARE(trip.averageSpeed(), 0.0);

    repo.set("trip:counter", "distance-m", "1234567890");
    repo.set("trip:counter", "duration-s", QString::number(std::numeric_limits<int>::max()));
    repo.set("trip:counter", "average-speed-kmh", "9876.5");
    QCOMPARE(trip.distance(), 1234567.89);
    QCOMPARE(trip.duration(), std::numeric_limits<int>::max());
    QCOMPARE(trip.averageSpeed(), 9876.5);
}

void SyncableStoreSeedTest::tripCounterSimulatorOverrideStaysSeparate()
{
    RecordingRepository repo;
    repo.set("trip:counter", "api-version", "1", false);
    repo.set("trip:counter", "distance-m", "2000", false);
    EngineStore engine(&repo);
    VehicleStore vehicle(&repo);
    TripStore trip(&repo, &engine, &vehicle);
    trip.start();

    trip.setOverride(9.5, 12, 34.0);
    repo.set("trip:counter", "distance-m", "3000");
    QCOMPARE(trip.distance(), 9.5);
    QVERIFY(repo.pushed.isEmpty());
    trip.clearOverride();
    QCOMPARE(trip.distance(), 3.0);
}

QTEST_GUILESS_MAIN(SyncableStoreSeedTest)
#include "SyncableStoreSeedTest.moc"
