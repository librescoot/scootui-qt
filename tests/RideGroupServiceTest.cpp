#include "services/RideGroupService.h"
#include "repositories/InMemoryMdbRepository.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

namespace {
class Repository : public InMemoryMdbRepository {
public:
  QList<QJsonObject> messages;
  void publish(const QString &channel, const QString &payload) override {
    if (channel == "ride-group:activity")
      messages.append(QJsonDocument::fromJson(payload.toUtf8()).object());
    InMemoryMdbRepository::publish(channel, payload);
  }
};
class DeferredRepository : public Repository {
public:
  struct Pending {
    QString channel, message;
    QDeadlineTimer deadline;
    std::function<bool()> permit;
  };
  QList<Pending> pending;
  void publishTransient(const QString &channel, const QString &message,
                        QDeadlineTimer deadline,
                        std::function<bool()> permit) override {
    pending.append({channel, message, deadline, std::move(permit)});
  }
  bool deliver(const Pending &p) {
    if (p.deadline.hasExpired() || !p.permit())
      return false;
    publish(p.channel, p.message);
    return true;
  }
};
QString id(int n) {
  return QString("00000000-0000-4000-8000-%1").arg(n, 12, 10, QChar('0'));
}
void send(RideGroupService &service, QJsonObject value) {
  service.receive(
      "ride-group:control",
      QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
}
QJsonObject lastState(const Repository &repo) {
  for (auto i = repo.messages.crbegin(); i != repo.messages.crend(); ++i)
    if (i->value("type") == "ride_group_local_stop")
      return *i;
  return {};
}
QJsonObject probe(QString adapter, int challenge) {
  return {{"v", 2},
          {"type", "ride_group_local_stop_probe"},
          {"adapter", adapter},
          {"challenge", id(challenge)}};
}
QJsonObject offer(QJsonObject state, QString adapter, int challenge,
                  int fence) {
  return {{"v", 2},
          {"type", "ride_group_local_stop_result"},
          {"adapter", adapter},
          {"operation_id", state["operation_id"]},
          {"journal_revision", state["journal_revision"]},
          {"challenge", id(challenge)},
          {"connection_nonce", id(80)},
          {"result", "offer"},
          {"expected_generation", fence},
          {"committed_generation", QJsonValue(QJsonValue::Null)}};
}
} // namespace
QJsonObject fixture(const QString &name) {
  QFile file(QFINDTESTDATA("data/ride-group/v2/") + name);
  if (!file.open(QIODevice::ReadOnly))
    qFatal("fixture unavailable");
  return QJsonDocument::fromJson(file.readAll()).object();
}
class RideGroupServiceTest : public QObject {
  Q_OBJECT
private slots:
  void peersRequireFreshAuthorityAndRetainTerminals() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    auto grant = fixture("control-grant.json");
    grant["server_time_ms"] = double(QDateTime::currentMSecsSinceEpoch());
    send(service, grant);
    auto point = fixture("peer-position.json");
    double now = QDateTime::currentMSecsSinceEpoch();
    point["captured_at_ms"] = now - 2000;
    point["received_at_ms"] = now - 100;
    point["server_time_ms"] = now;
    auto publish = [&](QJsonObject p) {
      service.receive(
          "ride-group:peers",
          QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));
    };
    publish(point);
    QCOMPARE(service.peerCount(), 1);
    auto terminal = fixture("peer-status.json");
    publish(terminal);
    QCOMPARE(service.peerCount(), 0);
    point["captured_at_ms"] = now - 1000;
    point["seq"] = 13;
    publish(point);
    QCOMPARE(service.peerCount(), 0);
    point["peer_transport_epoch"] = 9;
    point["peer_stream_id"] = id(70);
    publish(point);
    QCOMPARE(service.peerCount(), 1);
    service.stopSharing();
    QCOMPARE(service.peerCount(), 0);
    QTRY_VERIFY(!service.busy());
  }
  void futureSampleCannotPoisonFreshSource() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    send(service, fixture("control-grant.json"));
    auto point = fixture("peer-position.json");
    double now = QDateTime::currentMSecsSinceEpoch();
    point["captured_at_ms"] = now + 100000;
    point["received_at_ms"] = now + 100000;
    point["server_time_ms"] = now + 100000;
    service.receive(
        "ride-group:peers",
        QString::fromUtf8(QJsonDocument(point).toJson(QJsonDocument::Compact)));
    QCOMPARE(service.peerCount(), 0);
    point["captured_at_ms"] = now - 1000;
    point["received_at_ms"] = now;
    point["server_time_ms"] = now;
    service.receive(
        "ride-group:peers",
        QString::fromUtf8(QJsonDocument(point).toJson(QJsonDocument::Compact)));
    QCOMPARE(service.peerCount(), 1);
  }
  void predictionRequiresConfidentProducerPair() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    send(service, fixture("control-grant.json"));
    auto point = fixture("peer-position.json");
    double now = QDateTime::currentMSecsSinceEpoch();
    point["captured_at_ms"] = now - 2500;
    point["received_at_ms"] = now;
    point["server_time_ms"] = now;
    auto publish = [&] {
      service.receive("ride-group:peers",
                      QString::fromUtf8(
                          QJsonDocument(point).toJson(QJsonDocument::Compact)));
    };
    publish();
    point["captured_at_ms"] = now - 500;
    point["seq"] = 13;
    point["lat"] = 52.0002;
    publish();
    QTest::qWait(350);
    auto features = QJsonDocument::fromJson(service.peersGeoJson().toUtf8())
                        .object()["features"]
                        .toArray();
    QCOMPARE(features.size(), 3);
    QVERIFY(features.first()
                .toObject()["properties"]
                .toObject()["estimated"]
                .toBool());
    point["captured_at_ms"] = double(QDateTime::currentMSecsSinceEpoch());
    point["received_at_ms"] = point["captured_at_ms"];
    point["server_time_ms"] = point["captured_at_ms"];
    point["seq"] = 14;
    point["accuracy_m"] = QJsonValue(QJsonValue::Null);
    publish();
    QTest::qWait(350);
    features = QJsonDocument::fromJson(service.peersGeoJson().toUtf8())
                   .object()["features"]
                   .toArray();
    QCOMPARE(features.size(), 1);
    QVERIFY(!features.first()
                 .toObject()["properties"]
                 .toObject()["estimated"]
                 .toBool());
  }
  void queuedIdleCannotOutliveImmediateStopVeto() {
    DeferredRepository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    QVERIFY(!service.blocked());
    QVERIFY(!repo.pending.isEmpty());
    auto idle = repo.pending.last();
    QVERIFY(idle.permit());
    service.stopSharing();
    QVERIFY(!idle.permit());
    QVERIFY(!repo.deliver(idle));
    QTRY_VERIFY(!service.busy());
    auto fresh = repo.pending.last();
    QVERIFY(fresh.permit());
    emit repo.connectionStateChanged(false);
    QVERIFY(!fresh.permit());
    QVERIFY(!repo.deliver(fresh));
  }
  void defaultOffDoesNotCreateStorage() {
    Repository repo;
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QVERIFY(!service.enabled());
    QVERIFY(service.blocked());
    QVERIFY(!QFile::exists(root.path() + "/scootui/ride-group/owner.lock"));
    service.enableSharing();
    QVERIFY(repo.messages.isEmpty());
  }
  void missingStorageNeedsExplicitEnable() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    QVERIFY(service.blocked());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    QVERIFY(!service.blocked());
    QCOMPARE(service.phase(), QString("idle"));
  }
  void firstOfferPersistsImmutableFence() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    service.stopSharing();
    QVERIFY(service.blocked());
    QTRY_VERIFY(!service.busy());
    QCOMPARE(service.phase(), QString("preparing"));
    send(service, probe("mqtt", 10));
    auto preparing = lastState(repo);
    QCOMPARE(preparing["challenge"].toString(), id(10));
    send(service, offer(preparing, "mqtt", 10, 7));
    send(service, probe("uplink", 11));
    auto provisional = lastState(repo);
    QCOMPARE(provisional["phase"].toString(), QString("uncertain"));
    QCOMPARE(provisional["journal_revision"].toInt(), 0);
    send(service, offer(preparing, "uplink", 11, 9));
    QTRY_VERIFY(!service.busy());
    QCOMPARE(service.phase(), QString("prepared"));
    send(service, probe("mqtt", 12));
    auto prepared = lastState(repo);
    QCOMPARE(prepared["expected_generation"].toInt(), 7);
    QVERIFY(prepared["blocked"].toBool());
  }
  void recoveredPreparingNeverOffers() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    {
      auto journal = std::make_shared<RideGroupJournal>(root.path(), false);
      QVERIFY(journal->load().ok);
      QVERIFY(journal->save(
          {{"format", 1},
           {"revision", 1},
           {"blocked", true},
           {"operation_id", id(1)},
           {"phase", "preparing"},
           {"expected_generation", QJsonValue(QJsonValue::Null)},
           {"committed_generation", QJsonValue(QJsonValue::Null)}}));
    }
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    QCOMPARE(service.phase(), QString("uncertain"));
    send(service, probe("mqtt", 10));
    auto state = lastState(repo);
    QCOMPARE(state["phase"].toString(), QString("uncertain"));
    send(service, offer(state, "mqtt", 10, 7));
    QVERIFY(!service.busy());
    QCOMPARE(service.phase(), QString("uncertain"));
  }
  void committedReceiptPersistsAndRemainsBlocked() {
    Repository repo;
    repo.set("settings", "ride-group.enabled", "true");
    QTemporaryDir root;
    RideGroupService service(
        &repo, nullptr, std::make_shared<RideGroupJournal>(root.path(), false));
    QTRY_VERIFY(!service.busy());
    service.stopSharing();
    QTRY_VERIFY(!service.busy());
    send(service, probe("mqtt", 10));
    send(service, offer(lastState(repo), "mqtt", 10, 7));
    QTRY_VERIFY(!service.busy());
    send(service, probe("uplink", 11));
    auto state = lastState(repo);
    auto receipt = offer(state, "uplink", 11, 8);
    receipt["result"] = "committed";
    receipt["committed_generation"] = 9;
    send(service, receipt);
    QVERIFY(!service.busy());
    receipt["expected_generation"] = 7;
    receipt["committed_generation"] = 8;
    send(service, receipt);
    QTRY_VERIFY(!service.busy());
    QCOMPARE(service.phase(), QString("confirmed"));
    QVERIFY(service.blocked());
    service.enableSharing();
    QTRY_VERIFY(!service.busy());
    QVERIFY(!service.blocked());
  }
};
QTEST_GUILESS_MAIN(RideGroupServiceTest)
#include "RideGroupServiceTest.moc"
