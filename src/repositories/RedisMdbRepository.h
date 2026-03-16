#pragma once

#include "MdbRepository.h"
#include <QTcpSocket>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QByteArray>

// Synchronous Redis connection using RESP protocol over QTcpSocket
class RedisConnection : public QObject
{
    Q_OBJECT

public:
    explicit RedisConnection(const QString &host, quint16 port, QObject *parent = nullptr);
    ~RedisConnection() override;

    bool connectToServer(int timeoutMs = 2000);
    bool connectToServer(const QString &host, quint16 port, int timeoutMs = 2000);
    void disconnect();
    bool isConnected() const;

    // Send RESP command and get reply
    QVariant command(const QStringList &args, int timeoutMs = 2000);

    // Convenience wrappers
    QString hget(const QString &key, const QString &field);
    FieldMap hgetall(const QString &key);
    void hset(const QString &key, const QString &field, const QString &value);
    void hdel(const QString &key, const QString &field);
    void publish(const QString &channel, const QString &message);
    void lpush(const QString &key, const QString &value);
    QStringList smembers(const QString &key);
    QStringList lrange(const QString &key, int start, int stop);
    void sadd(const QString &key, const QString &member);
    void srem(const QString &key, const QString &member);

private:
    QByteArray buildCommand(const QStringList &args);
    QVariant readReply(int timeoutMs);
    QVariant parseReply();
    bool readLine(QByteArray &line, int timeoutMs);
    bool readBytes(int count, QByteArray &data, int timeoutMs);

    QTcpSocket *m_socket;
    QString m_host;
    quint16 m_port;
    QByteArray m_buffer;
};

// Pub/sub listener running in a dedicated thread
class RedisPubsubWorker : public QObject
{
    Q_OBJECT

public:
    explicit RedisPubsubWorker(const QString &host, quint16 port,
                               const QString &backupHost = QString());
    ~RedisPubsubWorker() override;

public slots:
    void start();
    void stop();
    void subscribe(const QString &channel);
    void unsubscribe(const QString &channel);
    void reconnectToPrimary();

signals:
    void messageReceived(const QString &channel, const QString &message);
    void connected();
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();
    void tryReconnect();

private:
    QVariant parseReply();

    RedisConnection *m_conn = nullptr;
    QTcpSocket *m_socket = nullptr;
    QString m_host;
    quint16 m_port;
    QString m_backupHost;
    QStringList m_channels;
    QByteArray m_buffer;
    QTimer *m_reconnectTimer = nullptr;
    bool m_running = false;
};

class RedisMdbRepository : public MdbRepository
{
    Q_OBJECT

public:
    explicit RedisMdbRepository(const QString &host = QStringLiteral("192.168.7.1"),
                                 quint16 port = 6379,
                                 const QString &backupHost = QStringLiteral("192.168.8.1"),
                                 QObject *parent = nullptr);
    ~RedisMdbRepository() override;

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

private slots:
    void onPubsubMessage(const QString &channel, const QString &message);
    void onPubsubConnected();
    void onPubsubDisconnected();

private:
    bool ensureConnected();
    void startReconnectTimer();
    void tryReconnectPrimary();

    RedisConnection *m_conn;
    QThread *m_pubsubThread;
    RedisPubsubWorker *m_pubsubWorker;
    QHash<QString, QList<SubscriptionCallback>> m_subscribers;
    QTimer *m_reconnectTimer;
    QString m_host;
    quint16 m_port;
    QString m_backupHost;
    bool m_connected = false;
    bool m_usingBackup = false;
    bool m_prolongedDisconnect = false;
    QTimer *m_prolongedTimer;
    QTimer *m_primaryReconnectTimer;
};
