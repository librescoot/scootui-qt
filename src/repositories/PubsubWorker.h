#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <hiredis/async.h>

class HiredisAdapter;

// Owns the Redis pub/sub connection on a thread of its own.
//
// It lives off the GUI thread because the connection has to come up while
// that thread is busy: on the dashboard the QML engine occupies the main
// event loop for seconds during startup, and a connect or retry parked on
// that loop cannot run until the load finishes. The link to the MDB is
// usable well before then, so pub/sub on the main thread arrives late and
// the UI shows defaults in the meantime.
//
// It is also kept off the polling worker's thread. That worker uses the
// synchronous hiredis API with a two second command timeout, so a single
// slow HGETALL there would stall pub/sub delivery for as long as it lasts.
//
// Subscriber callbacks are NOT run here. This class only tracks channel
// names and emits message() for the repository to dispatch on the GUI
// thread, where the stores and QML those callbacks touch actually live.
class PubsubWorker : public QObject
{
    Q_OBJECT

public:
    PubsubWorker(const QString &host, quint16 port, QObject *parent = nullptr);
    ~PubsubWorker() override;

public slots:
    // Connects and begins retrying. Wired to the thread's started() signal.
    void start();
    // Drops the connection and stops retrying. Call with a blocking queued
    // connection before quitting the thread.
    void stop();
    // Reconnects to a different host, for the failover the polling worker
    // decides. A no-op when the host has not changed.
    void setHost(const QString &host);
    void addChannel(const QString &channel);
    void removeChannel(const QString &channel);

signals:
    // One published message. Queued to the GUI thread for dispatch.
    void message(const QString &channel, const QString &payload);
    // Raised once a subscription set is live again. The repository re-reads
    // every subscribed hash on this, because anything published while there
    // was no subscription was missed.
    void subscriptionsLive();

private:
    void connectPubsub();
    void teardownPubsub();
    void subscribeOne(const QString &channel);
    void resubscribeAll();
    void scheduleReconnect();

    static void onConnected(const redisAsyncContext *ctx, int status);
    static void onDisconnected(const redisAsyncContext *ctx, int status);
    static void onReply(redisAsyncContext *ctx, void *reply, void *privdata);

    QString m_host;
    quint16 m_port;

    redisAsyncContext *m_ctx = nullptr;
    HiredisAdapter *m_adapter = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    QSet<QString> m_channels;

    // Set while teardownPubsub() drops the context on purpose, so the
    // disconnect callback does not schedule a reconnect on top of the
    // connect that is already in progress.
    bool m_tearingDown = false;
    bool m_running = false;
};
