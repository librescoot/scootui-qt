#include "RideGroupService.h"
#include "RideGroupJson.h"
#include "repositories/MdbRepository.h"
#include <QDateTime>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>
#include <cmath>

namespace {
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString json(const QJsonObject &o) {
  return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
QJsonValue null() { return QJsonValue(QJsonValue::Null); }
bool bounded(const QJsonValue &v, double low, double high) {
  return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= low &&
         v.toDouble() <= high;
}
const QStringList controlKeys = {"v",
                                 "type",
                                 "connection_nonce",
                                 "action",
                                 "session_id",
                                 "member_id",
                                 "revision",
                                 "transport_epoch",
                                 "stream_id",
                                 "challenge",
                                 "server_time_ms",
                                 "lease_ttl_ms",
                                 "cadence_ms",
                                 "peers",
                                 "consent_generation",
                                 "stop_operation_id",
                                 "stop_result",
                                 "stop_expected_generation"};
const QStringList peerKeys = {"v",
                              "type",
                              "session_id",
                              "revision",
                              "transport_epoch",
                              "stream_id",
                              "peer_stream_id",
                              "peer_transport_epoch",
                              "seq",
                              "peer_id",
                              "label",
                              "palette_index",
                              "captured_at_ms",
                              "timestamp_source",
                              "lat",
                              "lon",
                              "accuracy_m",
                              "heading_deg",
                              "speed_mps",
                              "motion_source",
                              "received_at_ms",
                              "server_time_ms",
                              "valid_for_ms"};
const QStringList statusKeys = {"v",
                                "type",
                                "session_id",
                                "revision",
                                "transport_epoch",
                                "stream_id",
                                "peer_id",
                                "peer_stream_id",
                                "peer_transport_epoch",
                                "status"};
} // namespace
RideGroupService::RideGroupService(MdbRepository *repo, QObject *parent,
                                   std::shared_ptr<RideGroupJournal> journal)
    : QObject(parent), m_repo(repo),
      m_journal(journal ? std::move(journal)
                        : std::make_shared<RideGroupJournal>()),
      m_boot(uuid()) {
  m_clock.start();
  m_link = repo->isConnected();
  for (const QString &channel :
       {QString("ride-group:control"), QString("ride-group:peers")})
    m_subscriptions.append(repo->subscribe(
        channel, [this](const QString &c, const QString &message) {
          receive(c, message);
        }));
  connect(repo, &MdbRepository::fieldsUpdated, this,
          [this](const QString &channel, const FieldMap &) {
            if (channel == "settings")
              configure();
          });
  connect(repo, &MdbRepository::connectionStateChanged, this, [this](bool up) {
    m_link = up;
    ++m_ioEpoch;
    m_ready = false;
    m_veto = true;
    m_intent = false;
    m_attempts.clear();
    clearLive();
    if (up && m_enabled)
      reload();
    emit changed();
  });
  connect(repo, &MdbRepository::dataSeeded, this, &RideGroupService::configure);
  m_heartbeat.setInterval(3000);
  connect(&m_heartbeat, &QTimer::timeout, this, &RideGroupService::heartbeat);
  m_heartbeat.start();
  m_animation.setInterval(50);
  connect(&m_animation, &QTimer::timeout, this, [this] {
    if (m_clock.elapsed() >= m_grantDeadline && !m_grant.isEmpty())
      clearLive();
    if (!m_peers.isEmpty())
      emit peersChanged();
  });
  configure();
}
RideGroupService::~RideGroupService() {
  ++*m_publicationEpoch;
  for (int i = 0; i < m_subscriptions.size(); ++i)
    m_repo->unsubscribe(i == 0 ? "ride-group:control" : "ride-group:peers",
                        m_subscriptions[i]);
}
void RideGroupService::configure() {
  bool enabled = m_repo->get("settings", "ride-group.enabled") == "true";
  if (enabled == m_enabled)
    return;
  m_enabled = enabled;
  m_intent = false;
  m_ready = false;
  m_veto = true;
  m_attempts.clear();
  clearLive();
  if (enabled && m_link)
    reload();
  emit changed();
}
void RideGroupService::reload() {
  if (m_busy) {
    m_reloadWanted = true;
    return;
  }
  ++*m_publicationEpoch;
  m_busy = true;
  m_ready = false;
  m_intent = false;
  m_veto = true;
  m_attempts.clear();
  emit changed();
  auto epoch = m_ioEpoch;
  auto *watcher = new QFutureWatcher<RideGroupJournalResult>(this);
  auto journal = m_journal;
  connect(watcher, &QFutureWatcher<RideGroupJournalResult>::finished, this,
          [this, watcher, epoch] {
            auto loaded = watcher->result();
            watcher->deleteLater();
            m_busy = false;
            if (epoch == m_ioEpoch && m_link && m_enabled && loaded.ok) {
              m_record = loaded.record;
              m_ready = true;
              m_veto = loaded.missing || loaded.record["blocked"].toBool(true);
              publishState();
            }
            emit changed();
            if (m_reloadWanted) {
              m_reloadWanted = false;
              if (m_link && m_enabled)
                reload();
            }
          });
  watcher->setFuture(QtConcurrent::run([journal] { return journal->load(); }));
}
void RideGroupService::save(QJsonObject record,
                            std::function<void(bool)> complete) {
  if (m_busy || !m_ready || !m_link) {
    complete(false);
    return;
  }
  ++*m_publicationEpoch;
  m_busy = true;
  emit changed();
  auto epoch = m_ioEpoch;
  auto *watcher = new QFutureWatcher<bool>(this);
  auto journal = m_journal;
  connect(watcher, &QFutureWatcher<bool>::finished, this,
          [this, watcher, record, complete, epoch] {
            bool ok = watcher->result();
            watcher->deleteLater();
            m_busy = false;
            ok = ok && epoch == m_ioEpoch && m_link && m_enabled;
            if (ok)
              m_record = record;
            else {
              m_ready = false;
              m_veto = true;
              m_intent = false;
              m_attempts.clear();
              clearLive();
            }
            complete(ok);
            emit changed();
            if (m_reloadWanted) {
              m_reloadWanted = false;
              if (m_link && m_enabled)
                reload();
            }
          });
  watcher->setFuture(
      QtConcurrent::run([journal, record] { return journal->save(record); }));
}
void RideGroupService::stopSharing() {
  m_veto = true;
  m_intent = false;
  m_attempts.clear();
  clearLive();
  heartbeat();
  emit changed();
  if (!m_enabled || !m_ready || m_busy)
    return;
  double revision = m_record.isEmpty() ? 0 : m_record["revision"].toDouble();
  if (revision >= 9007199254740991.0)
    return;
  QJsonObject record{{"format", 1},
                     {"revision", revision + 1},
                     {"blocked", true},
                     {"operation_id", uuid()},
                     {"phase", "preparing"},
                     {"expected_generation", null()},
                     {"committed_generation", null()}};
  save(record, [this](bool ok) {
    if (ok) {
      m_intent = true;
      publishState();
    } else
      publishState();
  });
}
void RideGroupService::enableSharing() {
  if (!m_enabled || !m_ready || m_busy)
    return;
  double revision = m_record.isEmpty() ? 0 : m_record["revision"].toDouble();
  if (revision >= 9007199254740991.0)
    return;
  QJsonObject record{{"format", 1},
                     {"revision", revision + 1},
                     {"blocked", false},
                     {"operation_id", null()},
                     {"phase", "idle"},
                     {"expected_generation", null()},
                     {"committed_generation", null()}};
  m_intent = false;
  m_attempts.clear();
  save(record, [this](bool ok) {
    if (ok) {
      m_veto = false;
      publishState();
      heartbeat();
    }
  });
}
void RideGroupService::publishState(QString challenge) {
  if (!m_enabled || !m_link)
    return;
  if (challenge.isEmpty())
    challenge = uuid();
  bool provisional = !m_ready || m_busy || m_record.isEmpty() ||
                     (m_record["phase"] == "preparing" && !m_intent);
  QJsonObject message{
      {"v", 2},
      {"type", "ride_group_local_stop"},
      {"operation_id", provisional ? null() : m_record["operation_id"]},
      {"journal_revision", provisional ? QJsonValue(0) : m_record["revision"]},
      {"phase", provisional ? QJsonValue("uncertain") : m_record["phase"]},
      {"blocked", provisional || blocked()},
      {"expected_generation",
       provisional ? null() : m_record["expected_generation"]},
      {"committed_generation",
       provisional ? null() : m_record["committed_generation"]},
      {"challenge", challenge}};
  publishLive(message);
}
void RideGroupService::publishLive(const QJsonObject &message, bool activity) {
  auto epoch = m_publicationEpoch;
  auto ticket = m_activityTicket;
  quint64 expected = epoch->load(),
          sequence = activity ? ++*ticket : ticket->load();
  m_repo->publishTransient("ride-group:activity", json(message),
                           QDeadlineTimer(1000),
                           [epoch, ticket, expected, sequence, activity] {
                             return epoch->load() == expected &&
                                    (!activity || ticket->load() == sequence);
                           });
}
void RideGroupService::setMapVisible(bool visible) {
  m_mapVisible = visible;
  if (visible)
    m_lastVisibleAt = m_clock.elapsed();
  heartbeat();
  if (visible && !m_peers.isEmpty())
    m_animation.start();
  else
    m_animation.stop();
}
void RideGroupService::heartbeat() {
  if (!m_enabled || !m_link || m_sequence >= 9007199254740991.0)
    return;
  for (auto i = m_attempts.begin(); i != m_attempts.end();) {
    if (i->deadline <= m_clock.elapsed())
      i = m_attempts.erase(i);
    else
      ++i;
  }
  bool eligible = m_repo->get("vehicle", "state") == "parked" ||
                  m_repo->get("vehicle", "state") == "ready-to-drive";
  bool active = m_mapVisible && m_clock.elapsed() - m_lastVisibleAt < 10000 &&
                eligible && !blocked() && m_ready && !m_busy;
  if (auto *application =
          qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    active = active && application->applicationState() == Qt::ApplicationActive;
  if (m_grantDeadline <= m_clock.elapsed() && !m_grant.isEmpty())
    clearLive();
  QJsonObject message{
      {"v", 2},
      {"type", "ride_group_activity"},
      {"boot_nonce", m_boot},
      {"seq", ++m_sequence},
      {"active", active},
      {"local_stop", false},
      {"session_id", m_grant.isEmpty() ? null() : m_grant["session_id"]},
      {"member_id", m_grant.isEmpty() ? null() : m_grant["member_id"]},
      {"challenge", uuid()}};
  publishLive(message, true);
}
void RideGroupService::clearLive() {
  ++*m_publicationEpoch;
  m_grant = {};
  m_grantDeadline = 0;
  for (auto &p : m_peers) {
    p.hasFix = false;
    p.predict = false;
  }
  m_animation.stop();
  emit peersChanged();
}
void RideGroupService::receive(const QString &channel, const QString &message) {
  if (!m_enabled || !m_link)
    return;
  QJsonObject value;
  if (!RideGroupJson::parse(message.toUtf8(), value) || value["v"] != 2)
    return;
  QString type = value["type"].toString();
  if (channel == "ride-group:control") {
    if (type == "ride_group_local_stop_probe")
      probe(value);
    else if (type == "ride_group_local_stop_result")
      result(value);
    else if (type == "ride_group_control")
      control(value);
  } else if (channel == "ride-group:peers" &&
             (type == "ride_group_peer" || type == "ride_group_peer_status"))
    peer(value);
}
void RideGroupService::probe(const QJsonObject &value) {
  if (!RideGroupJson::keys(value, {"v", "type", "adapter", "challenge"}) ||
      !RideGroupJson::uuid(value["challenge"]))
    return;
  QString adapter = value["adapter"].toString();
  if (adapter != "mqtt" && adapter != "uplink")
    return;
  QString key = adapter + ":" + value["challenge"].toString();
  if (m_attempts.contains(key))
    return;
  int count = 0;
  for (auto i = m_attempts.begin(); i != m_attempts.end();) {
    if (i->deadline <= m_clock.elapsed())
      i = m_attempts.erase(i);
    else {
      if (i->adapter == adapter)
        ++count;
      ++i;
    }
  }
  if (count >= 10)
    return;
  Attempt attempt;
  attempt.adapter = adapter;
  attempt.challenge = value["challenge"].toString();
  attempt.operation = m_record["operation_id"].toString();
  attempt.revision = m_record["revision"].toDouble();
  attempt.fence = m_record["expected_generation"];
  attempt.deadline = m_clock.elapsed() + 10000;
  m_attempts.insert(key, attempt);
  publishState(attempt.challenge);
}
void RideGroupService::result(const QJsonObject &value) {
  if (!RideGroupJson::keys(
          value, {"v", "type", "adapter", "operation_id", "journal_revision",
                  "challenge", "connection_nonce", "result",
                  "expected_generation", "committed_generation"}) ||
      !RideGroupJson::uuid(value["operation_id"]) ||
      !RideGroupJson::uuid(value["challenge"]) ||
      !RideGroupJson::uuid(value["connection_nonce"]) ||
      !RideGroupJson::counter(value["journal_revision"], false) ||
      !(value["expected_generation"].isNull() ||
        RideGroupJson::counter(value["expected_generation"])) ||
      !(value["committed_generation"].isNull() ||
        RideGroupJson::counter(value["committed_generation"], false)))
    return;
  QString key =
      value["adapter"].toString() + ":" + value["challenge"].toString();
  auto i = m_attempts.find(key);
  if (i == m_attempts.end() || i->consumed ||
      i->deadline <= m_clock.elapsed() || !m_ready || m_busy ||
      i->operation != value["operation_id"].toString() ||
      i->operation != m_record["operation_id"].toString() ||
      i->revision != value["journal_revision"].toDouble() ||
      i->revision != m_record["revision"].toDouble())
    return;
  QString nonce = value["connection_nonce"].toString();
  if (!i->nonce.isEmpty() && i->nonce != nonce)
    return;
  i->nonce = nonce;
  QString outcome = value["result"].toString();
  if (outcome == "offer") {
    if (!m_intent || m_record["phase"] != "preparing" ||
        !RideGroupJson::counter(value["expected_generation"]) ||
        !value["committed_generation"].isNull())
      return;
    m_intent = false;
    i->consumed = true;
    QJsonObject record = m_record;
    record["revision"] = record["revision"].toDouble() + 1;
    record["phase"] = "prepared";
    record["expected_generation"] = value["expected_generation"];
    m_attempts.clear();
    save(record, [this](bool) { publishState(); });
    return;
  }
  if (outcome == "committed") {
    if (m_record["phase"] == "confirmed" || m_record["phase"] == "preparing" ||
        !RideGroupJson::counter(value["expected_generation"]) ||
        !RideGroupJson::counter(value["committed_generation"], false) ||
        value["committed_generation"].toDouble() <=
            value["expected_generation"].toDouble() ||
        (!m_record["expected_generation"].isNull() &&
         m_record["expected_generation"] != value["expected_generation"]))
      return;
    i->consumed = true;
    QJsonObject record = m_record;
    record["revision"] = record["revision"].toDouble() + 1;
    record["phase"] = "confirmed";
    record["expected_generation"] = value["expected_generation"];
    record["committed_generation"] = value["committed_generation"];
    m_attempts.clear();
    save(record, [this](bool) { publishState(); });
  }
}
void RideGroupService::control(const QJsonObject &value) {
  if (!RideGroupJson::keys(value, controlKeys) ||
      !RideGroupJson::uuid(value["connection_nonce"]) ||
      !RideGroupJson::uuid(value["challenge"]) ||
      !RideGroupJson::counter(value["revision"]) ||
      !RideGroupJson::counter(value["transport_epoch"]) ||
      !RideGroupJson::counter(value["consent_generation"]) ||
      !RideGroupJson::counter(value["server_time_ms"], false) ||
      !RideGroupJson::counter(value["lease_ttl_ms"]))
    return;
  if (!value["stop_operation_id"].isNull() || !value["stop_result"].isNull() ||
      !value["stop_expected_generation"].isNull())
    return;
  QString action = value["action"].toString();
  if (action != "grant") {
    if ((action == "clear" || action == "status") &&
        m_grant["connection_nonce"] == value["connection_nonce"])
      clearLive();
    return;
  }
  if (blocked() || !m_ready || m_busy ||
      !RideGroupJson::uuid(value["session_id"]) ||
      !RideGroupJson::uuid(value["member_id"]) ||
      !RideGroupJson::uuid(value["stream_id"]) ||
      !RideGroupJson::counter(value["revision"], false) ||
      !RideGroupJson::counter(value["transport_epoch"], false) ||
      value["lease_ttl_ms"].toDouble() < 1 ||
      value["lease_ttl_ms"].toDouble() > 10000 ||
      !RideGroupJson::counter(value["cadence_ms"], false) ||
      !bounded(value["cadence_ms"], 1000, 5000) || !value["peers"].isArray() ||
      value["peers"].toArray().size() > 5)
    return;
  QString scope =
      value["session_id"].toString() + ":" + value["member_id"].toString();
  for (auto i = m_retiredScopes.begin(); i != m_retiredScopes.end();) {
    if (i.value() <= m_clock.elapsed())
      i = m_retiredScopes.erase(i);
    else
      ++i;
  }
  bool newScope = scope != m_scope;
  if (newScope &&
      (m_retiredScopes.contains(scope) ||
       (!m_grant.isEmpty() && m_clock.elapsed() < m_grantDeadline) ||
       (!m_scope.isEmpty() && m_retiredScopes.size() >= 5)))
    return;
  if (!newScope && (value["revision"].toDouble() < m_revisionFloor ||
                    value["transport_epoch"].toDouble() < m_epochFloor ||
                    value["consent_generation"].toDouble() < m_generationFloor))
    return;
  if (!m_grant.isEmpty() &&
      value["transport_epoch"] == m_grant["transport_epoch"] &&
      (value["stream_id"] != m_grant["stream_id"] ||
       value["connection_nonce"] != m_grant["connection_nonce"]))
    return;
  QHash<QString, QJsonObject> roster;
  for (const auto &entry : value["peers"].toArray()) {
    if (!entry.isObject())
      return;
    auto descriptor = entry.toObject();
    if (!RideGroupJson::keys(descriptor,
                             {"peer_id", "label", "palette_index"}) ||
        !RideGroupJson::uuid(descriptor["peer_id"]) ||
        !descriptor["label"].isString() ||
        QJsonDocument(QJsonArray{descriptor["label"]})
                    .toJson(QJsonDocument::Compact)
                    .size() -
                4 >
            32 ||
        !RideGroupJson::counter(descriptor["palette_index"]) ||
        descriptor["palette_index"].toInt() > 5 ||
        roster.contains(descriptor["peer_id"].toString()))
      return;
    roster.insert(descriptor["peer_id"].toString(), descriptor);
  }
  auto peers = newScope ? QHash<QString, Peer>{} : m_peers;
  for (auto i = peers.begin(); i != peers.end(); ++i)
    if (!roster.contains(i.key())) {
      i->descriptor = {};
      i->hasFix = false;
      i->terminal = true;
    }
  for (auto i = roster.begin(); i != roster.end(); ++i) {
    if (!peers.contains(i.key()) && peers.size() >= 5)
      return;
    peers[i.key()].descriptor = i.value();
  }
  if (newScope && !m_scope.isEmpty())
    m_retiredScopes.insert(m_scope, m_clock.elapsed() + 10000);
  m_scope = scope;
  m_peers = peers;
  m_revisionFloor = value["revision"].toDouble();
  m_epochFloor = value["transport_epoch"].toDouble();
  m_generationFloor = value["consent_generation"].toDouble();
  m_grant = value;
  m_grantDeadline =
      m_clock.elapsed() + qint64(value["lease_ttl_ms"].toDouble());
  if (m_mapVisible)
    m_animation.start();
  emit peersChanged();
}
bool RideGroupService::currentPeerEnvelope(const QJsonObject &value) const {
  return !blocked() && !m_grant.isEmpty() &&
         m_clock.elapsed() < m_grantDeadline &&
         value["session_id"] == m_grant["session_id"] &&
         value["revision"] == m_grant["revision"] &&
         value["transport_epoch"] == m_grant["transport_epoch"] &&
         value["stream_id"] == m_grant["stream_id"] &&
         RideGroupJson::uuid(value["peer_id"]) &&
         RideGroupJson::uuid(value["peer_stream_id"]) &&
         RideGroupJson::counter(value["peer_transport_epoch"], false);
}
void RideGroupService::peer(const QJsonObject &value) {
  bool status = value["type"] == "ride_group_peer_status";
  if (!RideGroupJson::keys(value, status ? statusKeys : peerKeys) ||
      !currentPeerEnvelope(value))
    return;
  auto i = m_peers.find(value["peer_id"].toString());
  if (i == m_peers.end() || i->descriptor.isEmpty())
    return;
  double epoch = value["peer_transport_epoch"].toDouble();
  QString stream = value["peer_stream_id"].toString();
  if (epoch < i->epoch ||
      (epoch == i->epoch && !i->stream.isEmpty() && stream != i->stream))
    return;
  if (status) {
    QString state = value["status"].toString();
    if (state == "stale" && epoch == i->epoch) {
      i->stale = true;
      i->predict = false;
      emit peersChanged();
    }
    if (state == "inactive" || state == "cleared" || state == "no_fix") {
      i->epoch = epoch;
      i->stream = stream;
      i->terminal = true;
      i->hasFix = false;
      i->predict = false;
      emit peersChanged();
    }
    return;
  }
  if ((epoch == i->epoch && i->terminal) ||
      !RideGroupJson::counter(value["seq"], false) ||
      (epoch == i->epoch && value["seq"].toDouble() <= i->sequence) ||
      !RideGroupJson::counter(value["captured_at_ms"], false) ||
      value["captured_at_ms"].toDouble() <= i->captured ||
      !bounded(value["lat"], -90, 90) || !bounded(value["lon"], -180, 180) ||
      !(value["accuracy_m"].isNull() ||
        bounded(value["accuracy_m"], 0.000001, 100000)) ||
      value["timestamp_source"] != "producer_fix" ||
      !RideGroupJson::counter(value["valid_for_ms"], false) ||
      value["valid_for_ms"].toDouble() > 5000 ||
      value["label"] != i->descriptor["label"] ||
      value["palette_index"] != i->descriptor["palette_index"])
    return;
  if (!RideGroupJson::counter(value["received_at_ms"], false) ||
      !RideGroupJson::counter(value["server_time_ms"], false) ||
      value["received_at_ms"].toDouble() < value["captured_at_ms"].toDouble() ||
      value["server_time_ms"].toDouble() < value["received_at_ms"].toDouble())
    return;
  QString motion = value["motion_source"].toString();
  if (motion == "unknown") {
    if (!value["heading_deg"].isNull() || !value["speed_mps"].isNull())
      return;
  } else if (motion == "producer_pair") {
    if (!bounded(value["heading_deg"], 0, 359.999999999) ||
        !bounded(value["speed_mps"], 0, 40))
      return;
  } else
    return;
  double now = QDateTime::currentMSecsSinceEpoch(),
         captured = value["captured_at_ms"].toDouble();
  if (captured > now || now - captured > 5000)
    return;
  double lat = value["lat"].toDouble(), lon = value["lon"].toDouble(),
         accuracy =
             value["accuracy_m"].isNull() ? -1 : value["accuracy_m"].toDouble();
  double dt = (captured - i->captured) / 1000.0;
  double north = qDegreesToRadians(lat - i->lat) * 6371000.0,
         east = qDegreesToRadians(std::remainder(lon - i->lon, 360.0)) *
                6371000.0 * qCos(qDegreesToRadians(lat));
  double displacement = std::hypot(north, east);
  bool predict = i->hasFix && epoch == i->epoch && dt >= 1 && dt <= 5 &&
                 accuracy > 0 && i->accuracy > 0 &&
                 displacement > accuracy + i->accuracy &&
                 displacement / dt <= 40;
  double oldSeconds =
      i->predict
          ? qMin(i->producerAge + (m_clock.elapsed() - i->received) / 1000.0,
                 3.0)
          : 0;
  double oldSpeed = std::hypot(i->north, i->east);
  if (oldSpeed * oldSeconds > 50)
    oldSeconds = 50 / oldSpeed;
  double producerAge = (now - captured) / 1000.0;
  double nextSeconds = predict ? qMin(producerAge, 3.0) : 0;
  double nextNorth = predict ? north / dt : 0,
         nextEast = predict ? east / dt : 0;
  double nextSpeed = std::hypot(nextNorth, nextEast);
  if (nextSpeed * nextSeconds > 50)
    nextSeconds = 50 / nextSpeed;
  double offsetNorth = qDegreesToRadians(i->lat - lat) * 6371000 +
                       i->north * oldSeconds - nextNorth * nextSeconds;
  double offsetEast = qDegreesToRadians(std::remainder(i->lon - lon, 360.0)) *
                          6371000 * qCos(qDegreesToRadians(lat)) +
                      i->east * oldSeconds - nextEast * nextSeconds;
  bool reconcile = i->hasFix && !i->terminal && epoch == i->epoch &&
                   std::hypot(offsetNorth, offsetEast) <= 50;
  i->reconcileNorth = reconcile ? offsetNorth : 0;
  i->reconcileEast = reconcile ? offsetEast : 0;
  i->producerAge = producerAge;
  i->predict = predict;
  i->stale = false;
  i->north = predict ? north / dt : 0;
  i->east = predict ? east / dt : 0;
  i->lat = lat;
  i->lon = lon;
  i->accuracy = accuracy;
  i->captured = captured;
  i->received = m_clock.elapsed();
  i->validUntil = i->received + qint64(value["valid_for_ms"].toDouble());
  i->epoch = epoch;
  i->stream = stream;
  i->sequence = value["seq"].toDouble();
  i->terminal = false;
  i->hasFix = true;
  emit peersChanged();
}
int RideGroupService::peerCount() const {
  int count = 0;
  for (const auto &p : m_peers)
    if (p.hasFix && m_clock.elapsed() - p.received < 30000)
      ++count;
  return count;
}
QString RideGroupService::peersGeoJson() const {
  QJsonArray features;
  for (const auto &p : m_peers) {
    if (!p.hasFix || m_clock.elapsed() - p.received >= 30000)
      continue;
    double elapsed = (m_clock.elapsed() - p.received) / 1000.0;
    double age = p.producerAge + elapsed;
    bool stale = p.stale || age >= 10;
    double seconds = !stale && p.predict ? qMin(age, 3.0) : 0;
    double speed = std::hypot(p.north, p.east);
    if (speed * seconds > 50)
      seconds = 50 / speed;
    double reconciliation = !stale ? qMax(0.0, 1.0 - elapsed / 0.3) : 0;
    bool estimated =
        seconds > 0 || (reconciliation > 0 &&
                        std::hypot(p.reconcileNorth, p.reconcileEast) > 0.01);
    double projectedNorth =
        p.north * seconds + p.reconcileNorth * reconciliation;
    double projectedEast = p.east * seconds + p.reconcileEast * reconciliation;
    double projectedDistance = std::hypot(projectedNorth, projectedEast);
    if (projectedDistance > 50) {
      projectedNorth *= 50 / projectedDistance;
      projectedEast *= 50 / projectedDistance;
    }
    double latitude = qBound(
               -90.0, p.lat + qRadiansToDegrees(projectedNorth / 6371000.0),
               90.0),
           cosine = qCos(qDegreesToRadians(p.lat));
    double longitude =
        p.lon + (qAbs(cosine) > 0.01
                     ? qRadiansToDegrees(projectedEast / (6371000.0 * cosine))
                     : 0);
    longitude = std::remainder(longitude, 360.0);
    QJsonObject properties = p.descriptor;
    properties["stale"] = stale;
    properties["estimated"] = estimated;
    properties["current"] = true;
    if (estimated)
      properties["label"] =
          QStringLiteral("≈") + properties["label"].toString();
    features.append(QJsonObject{
        {"type", "Feature"},
        {"geometry",
         QJsonObject{{"type", "Point"},
                     {"coordinates", QJsonArray{longitude, latitude}}}},
        {"properties", properties}});
    if (estimated) {
      properties["estimated"] = false;
      properties["current"] = false;
      properties["label"] = p.descriptor["label"];
      features.append(QJsonObject{
          {"type", "Feature"},
          {"geometry", QJsonObject{{"type", "Point"},
                                   {"coordinates", QJsonArray{p.lon, p.lat}}}},
          {"properties", properties}});
      features.append(QJsonObject{
          {"type", "Feature"},
          {"geometry",
           QJsonObject{
               {"type", "LineString"},
               {"coordinates", QJsonArray{QJsonArray{p.lon, p.lat},
                                          QJsonArray{longitude, latitude}}}}},
          {"properties", properties}});
    }
  }
  return json(
      QJsonObject{{"type", "FeatureCollection"}, {"features", features}});
}
