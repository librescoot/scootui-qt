#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QString>
#include <QStringList>

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

    // Read operations that need a return value (queued from main thread)
    // Results emitted via signals
    void doGetSetMembers(const QString &setKey);
    void doLrange(const QString &key, int start, int stop);

signals:
    // Emitted on the worker thread; received on the main thread via queued connection
    void fieldsUpdated(const QString &channel, const FieldMap &fields);
    void connectionChanged(bool connected, bool usingBackup);
    void setMembersResult(const QString &setKey, const QStringList &members);
    void lrangeResult(const QString &key, const QStringList &values);

private:
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
