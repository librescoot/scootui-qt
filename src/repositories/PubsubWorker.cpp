#include "PubsubWorker.h"
#include "HiredisAdapter.h"

#include <QDebug>

namespace {
// Long enough not to add load to a board that is saturated during startup,
// short enough that the first window after the link comes up is not missed.
constexpr int kReconnectIntervalMs = 500;
}

PubsubWorker::PubsubWorker(const QString &host, quint16 port, QObject *parent)
    : QObject(parent), m_host(host), m_port(port)
{
}

PubsubWorker::~PubsubWorker()
{
    teardownPubsub();
}

void PubsubWorker::start()
{
    if (m_running) return;
    m_running = true;

    // Created here rather than in the constructor so the timer belongs to the
    // thread that will run it. A QTimer only fires on the event loop of the
    // thread it lives on, and this object is moved after construction.
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kReconnectIntervalMs);
    connect(m_reconnectTimer, &QTimer::timeout, this, &PubsubWorker::connectPubsub);

    connectPubsub();
}

void PubsubWorker::stop()
{
    m_running = false;
    if (m_reconnectTimer) m_reconnectTimer->stop();
    teardownPubsub();
}

void PubsubWorker::setHost(const QString &host)
{
    if (host == m_host) return;
    m_host = host;
    if (!m_running) return;

    teardownPubsub();
    connectPubsub();
}

void PubsubWorker::addChannel(const QString &channel)
{
    if (m_channels.contains(channel)) return;
    m_channels.insert(channel);
    subscribeOne(channel);
}

void PubsubWorker::removeChannel(const QString &channel)
{
    if (m_channels.remove(channel) == 0) return;
    if (m_ctx)
        redisAsyncCommand(m_ctx, nullptr, nullptr,
                          "UNSUBSCRIBE %s", channel.toUtf8().constData());
}

void PubsubWorker::connectPubsub()
{
    if (!m_running) return;

    teardownPubsub();

    m_ctx = redisAsyncConnect(m_host.toUtf8().constData(), m_port);
    if (!m_ctx || m_ctx->err) {
        if (m_ctx) {
            redisAsyncFree(m_ctx);
            m_ctx = nullptr;
        }
        scheduleReconnect();
        return;
    }

    m_ctx->data = this;
    redisAsyncSetConnectCallback(m_ctx, onConnected);
    redisAsyncSetDisconnectCallback(m_ctx, onDisconnected);

    m_adapter = new HiredisAdapter(this);
    m_adapter->attach(m_ctx);

    resubscribeAll();
}

void PubsubWorker::teardownPubsub()
{
    if (!m_ctx) return;

    // redisAsyncDisconnect triggers the disconnect callback, which fires
    // ev.cleanup and drops the adapter's notifiers. The adapter is parented
    // to this object and cleaned up by Qt.
    m_tearingDown = true;
    redisAsyncDisconnect(m_ctx);
    m_tearingDown = false;
    m_ctx = nullptr;
    m_adapter = nullptr;
}

void PubsubWorker::subscribeOne(const QString &channel)
{
    if (!m_ctx) return;
    redisAsyncCommand(m_ctx, onReply, this,
                      "SUBSCRIBE %s", channel.toUtf8().constData());
}

void PubsubWorker::resubscribeAll()
{
    for (const QString &channel : std::as_const(m_channels))
        subscribeOne(channel);
}

void PubsubWorker::scheduleReconnect()
{
    if (!m_running || m_tearingDown) return;
    if (m_reconnectTimer && !m_reconnectTimer->isActive())
        m_reconnectTimer->start();
}

void PubsubWorker::onConnected(const redisAsyncContext *ctx, int status)
{
    auto *self = static_cast<PubsubWorker *>(ctx->data);
    if (!self) return;

    if (status != REDIS_OK) {
        // The connect only failed now, after redisAsyncConnect() had already
        // handed back a context, which is the usual case when the datastore
        // is not listening yet. hiredis frees the context right after this
        // callback and skips the disconnect callback for a connection that
        // never came up, so this is the only place left to schedule a retry.
        self->m_ctx = nullptr;
        self->m_adapter = nullptr;
        self->scheduleReconnect();
        return;
    }

    qDebug() << "PubsubWorker: connected";
    emit self->subscriptionsLive();
}

void PubsubWorker::onDisconnected(const redisAsyncContext *ctx, int status)
{
    Q_UNUSED(status)
    auto *self = static_cast<PubsubWorker *>(ctx->data);
    if (!self) return;

    self->m_ctx = nullptr;
    // The adapter is cleaned up by the hiredis cleanup callback.
    self->m_adapter = nullptr;

    if (self->m_tearingDown) return;

    qDebug() << "PubsubWorker: disconnected, scheduling reconnect";
    self->scheduleReconnect();
}

void PubsubWorker::onReply(redisAsyncContext *ctx, void *reply, void *privdata)
{
    Q_UNUSED(ctx)
    auto *self = static_cast<PubsubWorker *>(privdata);
    if (!self || !reply) return;

    auto *r = static_cast<redisReply *>(reply);
    if (r->type != REDIS_REPLY_ARRAY || r->elements < 3) return;

    const QString type = QString::fromUtf8(r->element[0]->str, r->element[0]->len);
    if (type != QLatin1String("message")) return;

    emit self->message(QString::fromUtf8(r->element[1]->str, r->element[1]->len),
                       QString::fromUtf8(r->element[2]->str, r->element[2]->len));
}
