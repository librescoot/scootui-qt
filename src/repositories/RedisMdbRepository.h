#pragma once

#include "MdbRepository.h"
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QList>

class HiredisWorker;
class PubsubWorker;

class RedisMdbRepository : public MdbRepository
{
    Q_OBJECT

public:
    explicit RedisMdbRepository(const QString &host = QStringLiteral("192.168.7.1"),
                                 quint16 port = 6379,
                                 const QString &backupHost = QStringLiteral("192.168.8.1"),
                                 QObject *parent = nullptr);
    ~RedisMdbRepository() override;

    // MdbRepository interface (sync reads become cache reads, writes are fire-and-forget)
    QString get(const QString &channel, const QString &variable) override;
    FieldMap getAll(const QString &channel) override;
    void set(const QString &channel, const QString &variable,
             const QString &value, bool publish = true) override;
    void publish(const QString &channel, const QString &message) override;
    SubscriptionId subscribe(const QString &channel, SubscriptionCallback callback) override;
    void unsubscribe(const QString &channel, SubscriptionId id) override;
    void push(const QString &channel, const QString &command) override;
    void dashboardReady() override;
    void publishButtonEvent(const QString &event) override;

    QStringList getSetMembers(const QString &setKey) override;
    void addToSet(const QString &setKey, const QString &member) override;
    void removeFromSet(const QString &setKey, const QString &member) override;
    void hdel(const QString &key, const QString &field) override;
    QStringList lrange(const QString &key, int start, int stop) override;
    QVariantList xrevrange(const QString &key, int count) override;

    bool isConnected() const override { return m_connected; }
    bool isUsingBackupConnection() const override { return m_usingBackup; }
    bool isDataSeeded() const override { return m_dataSeeded; }

    // Insert-if-absent per channel, so a prefetch result never overwrites a
    // later fetch. Returns how many channels were inserted.
    int seedCache(const QHash<QString, FieldMap> &hashes, bool markSeeded);

    // Register a channel for periodic polling by the worker.
    // Call before start(). SyncableStore calls this during construction.
    void registerPollChannel(const QString &channel, int intervalMs);

    // Fetch a single field immediately (queues to worker thread)
    void requestField(const QString &channel, const QString &field) override;

    // Fetch all fields of a hash immediately (queues to worker thread)
    void requestAll(const QString &channel) override;

    // Start the worker thread and pub/sub. Call after all channels are registered.
    void startWorker();

    // Synchronous initial cache fill on the calling thread. Opens a short-lived
    // blocking redisContext, walks every registered channel with HGETALL, and
    // dispatches results through onFieldsUpdated() so subscribed stores receive
    // their field values before QML loads. Bails as soon as the elapsed time
    // exceeds deadlineMs so a missing or slow Redis can't stall startup;
    // anything not fetched in time is picked up by the worker thread's normal
    // poll loop. Call after registerPollChannel() and SyncableStore::start(),
    // before startWorker().
    void prewarmCache(int deadlineMs);

private slots:
    void onFieldsUpdated(const QString &channel, const FieldMap &fields);
    void onWorkerConnectionChanged(bool connected, bool usingBackup);

private:
    void refreshSubscribedChannels();
    void retargetPubsub();
    void dispatchPubsubMessage(const QString &channel, const QString &payload);

    // A hash or set write, either on its way to the worker or parked until the
    // worker has a connection to send it over. See dispatchWrite().
    struct PendingWrite {
        enum Op { Hset, Hdel, Sadd, Srem };
        Op op;
        QString key;
        QString field;
        QString value;
        bool publish;
    };

    void dispatchWrite(const PendingWrite &write);
    void sendWrite(const PendingWrite &write);
    void flushPendingWrites();

    // Room for a full seed of every hash the dashboard writes several times
    // over. Hitting the cap means the link has been down long enough that the
    // oldest values are no longer worth replaying.
    static constexpr int kMaxPendingWrites = 512;

    // Worker thread
    QThread *m_workerThread = nullptr;
    HiredisWorker *m_worker = nullptr;

    // Pub/sub, on a thread of its own. It has to connect and retry while the
    // QML engine owns the GUI thread during startup, and it is kept off the
    // polling worker's thread because that one blocks on synchronous hiredis
    // calls. Only channel names live there; the subscriber callbacks below
    // stay here, because they touch stores and QML.
    QThread *m_pubsubThread = nullptr;
    PubsubWorker *m_pubsub = nullptr;

    // Cached data from worker (updated via signal, read from main thread)
    mutable QMutex m_cacheMutex;
    QHash<QString, FieldMap> m_cache;

    struct SubscriptionEntry {
        SubscriptionId id;
        SubscriptionCallback callback;
    };

    // Subscriptions
    QHash<QString, QList<SubscriptionEntry>> m_subscribers;
    SubscriptionId m_nextSubscriptionId = 1;

    // Connection state
    QString m_host;
    quint16 m_port;
    QString m_backupHost;
    void markDataSeeded();

    bool m_connected = false;
    bool m_usingBackup = false;
    bool m_dataSeeded = false;

    // Prolonged disconnect tracking
    QTimer *m_prolongedTimer = nullptr;
    bool m_prolongedDisconnect = false;

    // Writes issued while the worker had no connection, oldest first
    QMutex m_pendingMutex;
    QList<PendingWrite> m_pendingWrites;

    // Set/lrange results (for the few sync callers that need them)
    QMutex m_resultMutex;
    QHash<QString, QStringList> m_setResults;
    QHash<QString, QStringList> m_lrangeResults;
    QHash<QString, QVariantList> m_streamResults;
};
