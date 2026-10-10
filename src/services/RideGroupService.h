#pragma once
#include "RideGroupJournal.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>
class MdbRepository;

class RideGroupService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(bool blocked READ blocked NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool storageAvailable READ storageAvailable NOTIFY changed)
  Q_PROPERTY(QString phase READ phase NOTIFY changed)
  Q_PROPERTY(QString peersGeoJson READ peersGeoJson NOTIFY peersChanged)
  Q_PROPERTY(int peerCount READ peerCount NOTIFY peersChanged)
public:
  explicit RideGroupService(MdbRepository *repository,
                            QObject *parent = nullptr,
                            std::shared_ptr<RideGroupJournal> journal = {});
  ~RideGroupService() override;
  bool enabled() const { return m_enabled; }
  bool blocked() const { return m_veto || m_record["blocked"].toBool(true); }
  bool busy() const { return m_busy; }
  bool storageAvailable() const { return m_ready; }
  QString phase() const {
    return m_ready ? m_record["phase"].toString() : QStringLiteral("uncertain");
  }
  QString peersGeoJson() const;
  int peerCount() const;
  Q_INVOKABLE void setMapVisible(bool visible);
  Q_INVOKABLE void stopSharing();
  Q_INVOKABLE void enableSharing();
  void receive(const QString &channel, const QString &message);
signals:
  void changed();
  void peersChanged();

private:
  struct Attempt {
    QString adapter, challenge, operation, nonce;
    double revision = 0;
    QJsonValue fence;
    qint64 deadline = 0;
    bool consumed = false;
  };
  struct Peer {
    QJsonObject descriptor;
    QString stream;
    double epoch = 0, sequence = 0, captured = 0, lat = 0, lon = 0,
           accuracy = -1;
    double north = 0, east = 0, producerAge = 0;
    double reconcileNorth = 0, reconcileEast = 0;
    qint64 received = 0, validUntil = 0;
    bool hasFix = false, terminal = false, predict = false, stale = false;
  };
  void configure();
  void reload();
  void save(QJsonObject record, std::function<void(bool)> complete);
  void publishState(QString challenge = {});
  void publishLive(const QJsonObject &message, bool activity = false);
  void heartbeat();
  void clearLive();
  void control(const QJsonObject &message);
  void peer(const QJsonObject &message);
  void probe(const QJsonObject &message);
  void result(const QJsonObject &message);
  bool currentPeerEnvelope(const QJsonObject &message) const;
  MdbRepository *m_repo;
  std::shared_ptr<RideGroupJournal> m_journal;
  QJsonObject m_record, m_grant;
  QHash<QString, Attempt> m_attempts;
  QHash<QString, qint64> m_activityChallenges;
  QHash<QString, Peer> m_peers;
  QList<quint64> m_subscriptions;
  QTimer m_heartbeat, m_animation;
  QElapsedTimer m_clock;
  std::shared_ptr<std::atomic<quint64>> m_publicationEpoch =
      std::make_shared<std::atomic<quint64>>(0);
  std::shared_ptr<std::atomic<quint64>> m_activityTicket =
      std::make_shared<std::atomic<quint64>>(0);
  QString m_boot, m_scope;
  QHash<QString, qint64> m_retiredScopes;
  double m_sequence = 0, m_revisionFloor = 0, m_epochFloor = 0,
         m_generationFloor = 0;
  qint64 m_grantDeadline = 0, m_lastVisibleAt = -10000;
  quint64 m_ioEpoch = 0;
  bool m_reloadWanted = false;
  bool m_enabled = false, m_ready = false, m_veto = true, m_busy = false,
       m_intent = false, m_mapVisible = false, m_link = false;
};
