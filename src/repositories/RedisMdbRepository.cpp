#include "RedisMdbRepository.h"
#include "HiredisWorker.h"
#include "HiredisAdapter.h"
#include <QDebug>
#include <QCoreApplication>

RedisMdbRepository::RedisMdbRepository(const QString &host, quint16 port,
                                         const QString &backupHost, QObject *parent)
    : MdbRepository(parent), m_host(host), m_port(port), m_backupHost(backupHost)
{
    m_prolongedTimer = new QTimer(this);
    m_prolongedTimer->setSingleShot(true);
    m_prolongedTimer->setInterval(5000);
    connect(m_prolongedTimer, &QTimer::timeout, this, [this]() {
        if (!m_connected && !m_prolongedDisconnect) {
            m_prolongedDisconnect = true;
            emit prolongedDisconnect(true);
        }
    });

    m_pubsubReconnectTimer = new QTimer(this);
    m_pubsubReconnectTimer->setSingleShot(true);
    m_pubsubReconnectTimer->setInterval(2000);
    connect(m_pubsubReconnectTimer, &QTimer::timeout, this, [this]() {
        setupPubsub();
    });
}

RedisMdbRepository::~RedisMdbRepository()
{
    teardownPubsub();

    if (m_workerThread) {
        QMetaObject::invokeMethod(m_worker, &HiredisWorker::stop, Qt::QueuedConnection);
        m_workerThread->quit();
        m_workerThread->wait(3000);
        delete m_worker;
    }
}

void RedisMdbRepository::registerPollChannel(const QString &channel, int intervalMs)
{
    if (!m_worker) {
        m_worker = new HiredisWorker(m_host, m_port, m_backupHost);
    }
    m_worker->registerChannel(channel, intervalMs);
}

void RedisMdbRepository::startWorker()
{
    if (!m_worker) return;

    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &HiredisWorker::start);

    // Field updates from worker -> main thread cache + dispatch
    connect(m_worker, &HiredisWorker::fieldsUpdated,
            this, &RedisMdbRepository::onFieldsUpdated, Qt::QueuedConnection);

    // Connection state from worker
    connect(m_worker, &HiredisWorker::connectionChanged,
            this, &RedisMdbRepository::onWorkerConnectionChanged, Qt::QueuedConnection);

    // Set/lrange results
    connect(m_worker, &HiredisWorker::setMembersResult,
            this, [this](const QString &key, const QStringList &members) {
        QMutexLocker lock(&m_resultMutex);
        m_setResults[key] = members;
    }, Qt::QueuedConnection);

    connect(m_worker, &HiredisWorker::lrangeResult,
            this, [this](const QString &key, const QStringList &values) {
        QMutexLocker lock(&m_resultMutex);
        m_lrangeResults[key] = values;
    }, Qt::QueuedConnection);

    m_workerThread->start();

    // Set up pub/sub on main thread
    setupPubsub();
}

// Cache reads (non-blocking, main thread)

QString RedisMdbRepository::get(const QString &channel, const QString &variable)
{
    QMutexLocker lock(&m_cacheMutex);
    return m_cache.value(channel).value(variable);
}

FieldMap RedisMdbRepository::getAll(const QString &channel)
{
    QMutexLocker lock(&m_cacheMutex);
    return m_cache.value(channel);
}

// Write operations (fire-and-forget to worker thread)

void RedisMdbRepository::set(const QString &channel, const QString &variable,
                              const QString &value, bool doPublish)
{
    // Update local cache immediately for responsive reads
    {
        QMutexLocker lock(&m_cacheMutex);
        m_cache[channel][variable] = value;
    }

    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, channel, variable, value, doPublish]() {
            w->doSet(channel, variable, value, doPublish);
        }, Qt::QueuedConnection);
    }
}

void RedisMdbRepository::push(const QString &channel, const QString &command)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, channel, command]() {
            w->doPush(channel, command);
        }, Qt::QueuedConnection);
    } else {
        qWarning() << "RedisMdbRepository::push: worker not started, dropping"
                    << channel << command;
    }
}

void RedisMdbRepository::publish(const QString &channel, const QString &message)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, channel, message]() {
            w->doPublish(channel, message);
        }, Qt::QueuedConnection);
    }
}

void RedisMdbRepository::dashboardReady()
{
    set(QStringLiteral("dashboard"), QStringLiteral("ready"), QStringLiteral("true"));
}

void RedisMdbRepository::publishButtonEvent(const QString &event)
{
    publish(QStringLiteral("dashboard"), event);
}

void RedisMdbRepository::hdel(const QString &key, const QString &field)
{
    {
        QMutexLocker lock(&m_cacheMutex);
        if (m_cache.contains(key))
            m_cache[key].remove(field);
    }

    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, key, field]() {
            w->doHdel(key, field);
        }, Qt::QueuedConnection);
    }
}

void RedisMdbRepository::addToSet(const QString &setKey, const QString &member)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, setKey, member]() {
            w->doAddToSet(setKey, member);
        }, Qt::QueuedConnection);
    }
}

void RedisMdbRepository::removeFromSet(const QString &setKey, const QString &member)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, setKey, member]() {
            w->doRemoveFromSet(setKey, member);
        }, Qt::QueuedConnection);
    }
}

// These are still synchronous reads for now (rare callers).
// They return the last cached result from the worker.
QStringList RedisMdbRepository::getSetMembers(const QString &setKey)
{
    // Trigger a fetch and return cached result
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, setKey]() {
            w->doGetSetMembers(setKey);
        }, Qt::QueuedConnection);
    }
    QMutexLocker lock(&m_resultMutex);
    return m_setResults.value(setKey);
}

QStringList RedisMdbRepository::lrange(const QString &key, int start, int stop)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, key, start, stop]() {
            w->doLrange(key, start, stop);
        }, Qt::QueuedConnection);
    }
    QMutexLocker lock(&m_resultMutex);
    return m_lrangeResults.value(key);
}

// Pub/sub (subscribe/unsubscribe)

void RedisMdbRepository::subscribe(const QString &channel, SubscriptionCallback callback)
{
    m_subscribers[channel].append(callback);

    if (m_pubsubCtx) {
        redisAsyncCommand(m_pubsubCtx, onPubsubReply, this,
                          "SUBSCRIBE %s", channel.toUtf8().constData());
    }
}

void RedisMdbRepository::unsubscribe(const QString &channel)
{
    m_subscribers.remove(channel);

    if (m_pubsubCtx) {
        redisAsyncCommand(m_pubsubCtx, nullptr, nullptr,
                          "UNSUBSCRIBE %s", channel.toUtf8().constData());
    }
}

// Worker signals -> main thread

void RedisMdbRepository::onFieldsUpdated(const QString &channel, const FieldMap &fields)
{
    // Update cache
    {
        QMutexLocker lock(&m_cacheMutex);
        m_cache[channel] = fields;
    }

    // Dispatch to subscribers as a "something changed" notification
    auto it = m_subscribers.find(channel);
    if (it != m_subscribers.end()) {
        for (const auto &cb : *it)
            cb(channel, QStringLiteral("*"));
    }

    // Also emit the generic fieldsUpdated for SyncableStore
    emit fieldsUpdated(channel, fields);
}

void RedisMdbRepository::onWorkerConnectionChanged(bool connected)
{
    bool wasConnected = m_connected;
    m_connected = connected;

    if (connected && !wasConnected) {
        m_prolongedTimer->stop();
        if (m_prolongedDisconnect) {
            m_prolongedDisconnect = false;
            emit prolongedDisconnect(false);
        }
        emit connectionStateChanged(true);
    } else if (!connected && wasConnected) {
        emit connectionStateChanged(false);
        m_prolongedTimer->start();
    }
}

// Pub/sub async context (main thread)

void RedisMdbRepository::setupPubsub()
{
    teardownPubsub();

    m_pubsubCtx = redisAsyncConnect(m_host.toUtf8().constData(), m_port);
    if (!m_pubsubCtx || m_pubsubCtx->err) {
        // Try backup
        if (!m_backupHost.isEmpty()) {
            if (m_pubsubCtx) redisAsyncFree(m_pubsubCtx);
            m_pubsubCtx = redisAsyncConnect(m_backupHost.toUtf8().constData(), m_port);
        }
        if (!m_pubsubCtx || m_pubsubCtx->err) {
            qWarning() << "RedisMdbRepository: pubsub connection failed";
            if (m_pubsubCtx) {
                redisAsyncFree(m_pubsubCtx);
                m_pubsubCtx = nullptr;
            }
            m_pubsubReconnectTimer->start();
            return;
        }
    }

    m_pubsubCtx->data = this;
    redisAsyncSetConnectCallback(m_pubsubCtx, onPubsubConnected);
    redisAsyncSetDisconnectCallback(m_pubsubCtx, onPubsubDisconnected);

    m_pubsubAdapter = new HiredisAdapter(this);
    m_pubsubAdapter->attach(m_pubsubCtx);

    resubscribeAll();
}

void RedisMdbRepository::teardownPubsub()
{
    if (m_pubsubCtx) {
        // redisAsyncDisconnect triggers the disconnect callback which
        // fires ev.cleanup, cleaning up the adapter's notifiers.
        // The adapter itself is parented to us and cleaned up by Qt.
        redisAsyncDisconnect(m_pubsubCtx);
        m_pubsubCtx = nullptr;
        m_pubsubAdapter = nullptr;
    }
}

void RedisMdbRepository::resubscribeAll()
{
    if (!m_pubsubCtx) return;

    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it) {
        redisAsyncCommand(m_pubsubCtx, onPubsubReply, this,
                          "SUBSCRIBE %s", it.key().toUtf8().constData());
    }
}

void RedisMdbRepository::onPubsubConnected(const redisAsyncContext *ctx, int status)
{
    if (status != REDIS_OK) {
        qWarning() << "RedisMdbRepository: pubsub connect failed";
        return;
    }
    qDebug() << "RedisMdbRepository: pubsub connected";
}

void RedisMdbRepository::onPubsubDisconnected(const redisAsyncContext *ctx, int status)
{
    auto *self = static_cast<RedisMdbRepository *>(ctx->data);
    if (!self) return;

    qDebug() << "RedisMdbRepository: pubsub disconnected, scheduling reconnect";
    self->m_pubsubCtx = nullptr;
    // Adapter is cleaned up by hiredis cleanup callback
    self->m_pubsubAdapter = nullptr;
    self->m_pubsubReconnectTimer->start();
}

void RedisMdbRepository::onPubsubReply(redisAsyncContext *ctx, void *reply, void *privdata)
{
    auto *self = static_cast<RedisMdbRepository *>(privdata);
    if (!self || !reply) return;

    auto *r = static_cast<redisReply *>(reply);
    if (r->type != REDIS_REPLY_ARRAY || r->elements < 3) return;

    QString type = QString::fromUtf8(r->element[0]->str, r->element[0]->len);
    if (type != QLatin1String("message")) return;

    QString channel = QString::fromUtf8(r->element[1]->str, r->element[1]->len);
    QString message = QString::fromUtf8(r->element[2]->str, r->element[2]->len);

    // Dispatch to subscribers
    auto it = self->m_subscribers.find(channel);
    if (it != self->m_subscribers.end()) {
        for (const auto &cb : *it)
            cb(channel, message);
    }
}
