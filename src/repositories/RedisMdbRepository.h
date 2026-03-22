#pragma once

#include "MdbRepository.h"
#include <QThread>
#include <QTimer>
#include <QMutex>

class HiredisWorker;
class HiredisAdapter;
struct redisAsyncContext;

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
    void subscribe(const QString &channel, SubscriptionCallback callback) override;
    void unsubscribe(const QString &channel) override;
    void push(const QString &channel, const QString &command) override;
    void dashboardReady() override;
    void publishButtonEvent(const QString &event) override;

    QStringList getSetMembers(const QString &setKey) override;
    void addToSet(const QString &setKey, const QString &member) override;
    void removeFromSet(const QString &setKey, const QString &member) override;
    void hdel(const QString &key, const QString &field) override;
    QStringList lrange(const QString &key, int start, int stop) override;

    bool isConnected() const override { return m_connected; }
    bool isUsingBackupConnection() const override { return m_usingBackup; }

    // Register a channel for periodic polling by the worker.
    // Call before start(). SyncableStore calls this during construction.
    void registerPollChannel(const QString &channel, int intervalMs);

    // Start the worker thread and pub/sub. Call after all channels are registered.
    void startWorker();

private slots:
    void onFieldsUpdated(const QString &channel, const FieldMap &fields);
    void onWorkerConnectionChanged(bool connected);

private:
    void setupPubsub();
    void teardownPubsub();
    void resubscribeAll();
    static void onPubsubConnected(const redisAsyncContext *ctx, int status);
    static void onPubsubDisconnected(const redisAsyncContext *ctx, int status);
    static void onPubsubReply(redisAsyncContext *ctx, void *reply, void *privdata);

    // Worker thread
    QThread *m_workerThread = nullptr;
    HiredisWorker *m_worker = nullptr;

    // Pub/sub (main thread, async)
    redisAsyncContext *m_pubsubCtx = nullptr;
    HiredisAdapter *m_pubsubAdapter = nullptr;
    QTimer *m_pubsubReconnectTimer = nullptr;

    // Cached data from worker (updated via signal, read from main thread)
    mutable QMutex m_cacheMutex;
    QHash<QString, FieldMap> m_cache;

    // Subscriptions
    QHash<QString, QList<SubscriptionCallback>> m_subscribers;

    // Connection state
    QString m_host;
    quint16 m_port;
    QString m_backupHost;
    bool m_connected = false;
    bool m_usingBackup = false;

    // Prolonged disconnect tracking
    QTimer *m_prolongedTimer = nullptr;
    bool m_prolongedDisconnect = false;

    // Set/lrange results (for the few sync callers that need them)
    QMutex m_resultMutex;
    QHash<QString, QStringList> m_setResults;
    QHash<QString, QStringList> m_lrangeResults;
};
