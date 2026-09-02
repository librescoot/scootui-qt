#pragma once

#include <QObject>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>

struct redisContext;

using FieldMap = QHash<QString, QString>;

// Polling registration: channel name + interval in ms
struct PollRegistration {
    QString channel;
    int intervalMs;
};

// Runs on a dedicated thread. Owns a synchronous redisContext and
// periodically polls registered channels via HGETALL. Write operations
// (HSET, LPUSH, PUBLISH, etc.) are invoked from the main thread and
// dispatched here via queued signal connections.
class HiredisWorker : public QObject
{
    Q_OBJECT

public:
    explicit HiredisWorker(const QString &host, quint16 port,
                           const QString &backupHost = QString());
    ~HiredisWorker() override;

    // Thread-safe: called from main thread before worker starts
    void registerChannel(const QString &channel, int intervalMs);

    // Snapshot of registered channel names (main thread, before worker starts).
    QStringList registeredChannels() const;

public slots:
    // Called on the worker thread
    void start();
    void stop();

    // Fetch all registered channels once (used for initial sync)
    void fetchAll();

    // Write operations (queued from main thread)
    void doSet(const QString &channel, const QString &variable,
               const QString &value, bool publish);
    void doPush(const QString &channel, const QString &command);
    void doPublish(const QString &channel, const QString &message);
    void doHdel(const QString &key, const QString &field);
    void doAddToSet(const QString &setKey, const QString &member);
    void doRemoveFromSet(const QString &setKey, const QString &member);

    // Fetch a single field from a hash (queued from main thread)
    void doHget(const QString &channel, const QString &field);

    // Fetch all fields of a hash on demand (HGETALL, queued from main thread).
    // Emits fieldsUpdated on the worker thread when the response arrives.
    void doHgetAll(const QString &channel);

    // Read operations that need a return value (queued from main thread)
    // Results emitted via signals
    void doGetSetMembers(const QString &setKey);
    void doLrange(const QString &key, int start, int stop);
    void doXrevrange(const QString &key, int count);

signals:
    // Emitted on the worker thread; received on the main thread via queued connection
    void fieldsUpdated(const QString &channel, const FieldMap &fields);
    void fieldFetched(const QString &channel, const QString &field, const QString &value);
    void connectionChanged(bool connected, bool usingBackup);
    // Every registered channel polled once since the current connection came up.
    void firstPassComplete();
    void setMembersResult(const QString &setKey, const QStringList &members);
    void lrangeResult(const QString &key, const QStringList &values);
    // Each entry is a QVariantMap with keys "id" (QString) and "fields" (QVariantMap).
    void streamResult(const QString &key, const QVariantList &entries);

private:
    // Arm the reconnect timer with the current backoff. Retries stay fast until
    // the first successful connection, because at boot a failure means Redis is
    // not up *yet*, not that it is down; once we have been connected once, a
    // failure is a real dropout and there is no point hammering it.
    void scheduleReconnect();

    static constexpr int kFastRetryMs = 250;
    static constexpr int kSlowRetryMs = 2000;
    // Roughly 30s of fast retries before giving up on the boot case.
    static constexpr int kFastRetryAttempts = 120;
    // The backup lives across the modem link, which comes up later than the
    // primary does. Trying it every cycle from the start would add its 1s
    // timeout to each retry and dominate the loop exactly when we want to be
    // quick, so leave it until the primary has really failed.
    static constexpr int kBackupAfterFailures = 8;

    bool ensureConnected();
    void disconnectRedis();
    void onPollTimer();
    void pollChannel(const QString &channel);
    void tryReconnect();

    redisContext *m_ctx = nullptr;
    QString m_host;
    quint16 m_port;
    QString m_backupHost;
    bool m_connected = false;
    bool m_usingBackup = false;
    bool m_running = false;
    bool m_everConnected = false;
    int m_consecutiveFailures = 0;
    QSet<QString> m_polledSinceConnect;
    bool m_firstPassReported = false;

    // Polling schedule: GCD-based single timer
    QTimer *m_pollTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_primaryProbeTimer = nullptr;
    int m_gcdInterval = 0;
    int m_tickCount = 0;

    struct ChannelPoll {
        QString channel;
        int intervalMs;
        int tickModulo; // poll when m_tickCount % tickModulo == 0
    };
    QList<ChannelPoll> m_channels;
};
