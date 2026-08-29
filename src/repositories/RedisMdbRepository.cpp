#include "RedisMdbRepository.h"
#include "HiredisWorker.h"
#include "PubsubWorker.h"
#include <QDebug>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <utility>
#include <hiredis/hiredis.h>
#include "RedisSchema.h"

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

}

RedisMdbRepository::~RedisMdbRepository()
{
    if (m_pubsubThread) {
        QMetaObject::invokeMethod(m_pubsub, &PubsubWorker::stop, Qt::BlockingQueuedConnection);
        m_pubsubThread->quit();
        m_pubsubThread->wait(3000);
        delete m_pubsub;
    }

    if (m_workerThread) {
        QMetaObject::invokeMethod(m_worker, &HiredisWorker::stop, Qt::BlockingQueuedConnection);
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

void RedisMdbRepository::requestField(const QString &channel, const QString &field)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, channel, field]() {
            w->doHget(channel, field);
        }, Qt::QueuedConnection);
    }
}

void RedisMdbRepository::requestAll(const QString &channel)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, channel]() {
            w->doHgetAll(channel);
        }, Qt::QueuedConnection);
    }
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

    // Single-field fetch results (from HGET after pub/sub notification)
    connect(m_worker, &HiredisWorker::fieldFetched,
            this, [this](const QString &channel, const QString &field, const QString &value) {
        {
            QMutexLocker lock(&m_cacheMutex);
            m_cache[channel][field] = value;
        }
        emit fieldFetched(channel, field, value);
    }, Qt::QueuedConnection);

    // Connection state from worker (bool connected, bool usingBackup)
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

    connect(m_worker, &HiredisWorker::streamResult,
            this, [this](const QString &key, const QVariantList &entries) {
        {
            QMutexLocker lock(&m_resultMutex);
            m_streamResults[key] = entries;
        }
        emit streamFetched(key, entries);
    }, Qt::QueuedConnection);

    m_workerThread->start();

    // Pub/sub runs on its own thread and starts connecting straight away,
    // rather than waiting for the polling worker to report a connection. On
    // the dashboard that wait cost seconds: the worker's signal is queued to
    // the GUI thread, which is inside the QML load, so pub/sub only came up
    // once that finished even though the link was ready much earlier.
    m_pubsub = new PubsubWorker(m_host, m_port);
    m_pubsubThread = new QThread(this);
    m_pubsub->moveToThread(m_pubsubThread);

    connect(m_pubsubThread, &QThread::started, m_pubsub, &PubsubWorker::start);

    connect(m_pubsub, &PubsubWorker::message,
            this, &RedisMdbRepository::dispatchPubsubMessage, Qt::QueuedConnection);
    connect(m_pubsub, &PubsubWorker::subscriptionsLive,
            this, &RedisMdbRepository::refreshSubscribedChannels, Qt::QueuedConnection);

    // Channels subscribed before now were only recorded locally, so hand the
    // whole set over before the thread starts serving them.
    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it) {
        auto *p = m_pubsub;
        const QString channel = it.key();
        QMetaObject::invokeMethod(p, [p, channel]() { p->addChannel(channel); },
                                  Qt::QueuedConnection);
    }

    m_pubsubThread->start();
}

void RedisMdbRepository::prewarmCache(int deadlineMs)
{
    if (!m_worker) return;

    QStringList channels = m_worker->registeredChannels();
    if (channels.isEmpty()) return;

    QElapsedTimer elapsed;
    elapsed.start();

    // Try primary first with a tight connect budget; fall back to backup if
    // we still have time. Both budgets stay within deadlineMs so a fully
    // unreachable Redis can't blow past the cap.
    int connectBudget = qMin(200, deadlineMs);
    struct timeval tv = {connectBudget / 1000, (connectBudget % 1000) * 1000};
    redisContext *ctx = redisConnectWithTimeout(
        m_host.toUtf8().constData(), m_port, tv);
    bool usingBackup = false;
    if ((!ctx || ctx->err) && !m_backupHost.isEmpty()) {
        if (ctx) { redisFree(ctx); ctx = nullptr; }
        int remaining = deadlineMs - static_cast<int>(elapsed.elapsed());
        if (remaining <= 0) {
            qDebug() << "prewarmCache: deadline hit before backup attempt";
            return;
        }
        int backupBudget = qMin(remaining, 500);
        tv = {backupBudget / 1000, (backupBudget % 1000) * 1000};
        ctx = redisConnectWithTimeout(
            m_backupHost.toUtf8().constData(), m_port, tv);
        usingBackup = true;
    }
    if (!ctx || ctx->err) {
        qDebug() << "prewarmCache: connect failed in"
                 << elapsed.elapsed() << "ms, leaving cache cold";
        if (ctx) redisFree(ctx);
        return;
    }

    // Cap any single HGETALL so a wedged hash can't eat the whole budget.
    int remaining = qMax(50, deadlineMs - static_cast<int>(elapsed.elapsed()));
    struct timeval cmdTv = {remaining / 1000, (remaining % 1000) * 1000};
    redisSetTimeout(ctx, cmdTv);

    int filled = 0;
    int skipped = 0;
    for (const QString &channel : channels) {
        if (elapsed.elapsed() >= deadlineMs) {
            skipped = channels.size() - (filled + 1);
            break;
        }

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(ctx, "HGETALL %s", channel.toUtf8().constData()));
        if (!reply) {
            qDebug() << "prewarmCache: HGETALL" << channel
                     << "failed:" << ctx->errstr;
            break;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
            FieldMap fields;
            fields.reserve(static_cast<int>(reply->elements / 2));
            for (size_t i = 0; i + 1 < reply->elements; i += 2) {
                fields.insert(
                    QString::fromUtf8(reply->element[i]->str,
                                       reply->element[i]->len),
                    QString::fromUtf8(reply->element[i + 1]->str,
                                       reply->element[i + 1]->len));
            }
            // Same path the worker uses: updates m_cache and emits
            // fieldsUpdated, which SyncableStores receive synchronously.
            onFieldsUpdated(channel, fields);
            ++filled;
        }
        freeReplyObject(reply);
    }

    redisFree(ctx);
    qDebug().nospace() << "prewarmCache: filled " << filled << "/"
                       << channels.size() << " channels in "
                       << elapsed.elapsed() << "ms"
                       << (skipped ? QStringLiteral(" (%1 skipped past deadline)").arg(skipped) : QString())
                       << (usingBackup ? " via backup" : "");
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

    dispatchWrite({PendingWrite::Hset, channel, variable, value, doPublish});
}

// Writes only reach Redis through the worker, and anything issued before the
// worker has a link used to vanish silently: the worker is not created until
// the first registerPollChannel(), and HiredisWorker drops commands it cannot
// send. Park those writes here and replay them in order once a link is up.
//
// Armed for every disconnected stretch, not only the first, because a dropped
// link loses writes the same way and the dashboard is the sole writer of the
// hashes involved, so replaying its last value is what the caller asked for.
// push() and publish() stay fire-and-forget: a command or a button event
// replayed minutes later would be wrong rather than merely late.
void RedisMdbRepository::dispatchWrite(const PendingWrite &write)
{
    {
        QMutexLocker lock(&m_pendingMutex);
        // A non-empty queue means a replay is still owed, so going direct here
        // would let this write overtake writes issued before it.
        if (!m_connected || !m_worker || !m_pendingWrites.isEmpty()) {
            // A later write to the same field replaces the earlier one. That
            // bounds the queue by the number of distinct fields touched and
            // makes the value that lands the one the caller wrote last.
            for (int i = 0; i < m_pendingWrites.size(); ++i) {
                if (m_pendingWrites.at(i).key == write.key
                    && m_pendingWrites.at(i).field == write.field) {
                    m_pendingWrites.removeAt(i);
                    break;
                }
            }

            if (m_pendingWrites.size() >= kMaxPendingWrites) {
                const PendingWrite dropped = m_pendingWrites.takeFirst();
                qWarning() << "RedisMdbRepository: write queue full, dropping"
                           << dropped.key << dropped.field;
            }

            m_pendingWrites.append(write);
            return;
        }
    }

    sendWrite(write);
}

void RedisMdbRepository::sendWrite(const PendingWrite &write)
{
    auto *w = m_worker;
    if (!w) return;

    QMetaObject::invokeMethod(w, [w, write]() {
        switch (write.op) {
        case PendingWrite::Hset:
            w->doSet(write.key, write.field, write.value, write.publish);
            break;
        case PendingWrite::Hdel:
            w->doHdel(write.key, write.field);
            break;
        case PendingWrite::Sadd:
            w->doAddToSet(write.key, write.field);
            break;
        case PendingWrite::Srem:
            w->doRemoveFromSet(write.key, write.field);
            break;
        }
    }, Qt::QueuedConnection);
}

void RedisMdbRepository::flushPendingWrites()
{
    // The lock is held across the whole drain so a write arriving meanwhile
    // queues behind the replay instead of racing ahead of it.
    QMutexLocker lock(&m_pendingMutex);
    if (m_pendingWrites.isEmpty()) return;

    qDebug() << "RedisMdbRepository: replaying" << m_pendingWrites.size()
             << "writes queued while disconnected";
    for (const PendingWrite &write : std::as_const(m_pendingWrites))
        sendWrite(write);
    m_pendingWrites.clear();
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
    set(RedisSchema::hash::Dashboard, QStringLiteral("ready"), QStringLiteral("true"));
}

void RedisMdbRepository::publishButtonEvent(const QString &event)
{
    publish(RedisSchema::hash::Dashboard, event);
}

void RedisMdbRepository::hdel(const QString &key, const QString &field)
{
    {
        QMutexLocker lock(&m_cacheMutex);
        if (m_cache.contains(key))
            m_cache[key].remove(field);
    }

    dispatchWrite({PendingWrite::Hdel, key, field, QString(), false});
}

void RedisMdbRepository::addToSet(const QString &setKey, const QString &member)
{
    dispatchWrite({PendingWrite::Sadd, setKey, member, QString(), false});
}

void RedisMdbRepository::removeFromSet(const QString &setKey, const QString &member)
{
    dispatchWrite({PendingWrite::Srem, setKey, member, QString(), false});
}

// Async fetch + cached read. The fetch is queued to the worker thread and
// the result from the *previous* fetch is returned immediately. This means
// the first call always returns an empty list; callers must tolerate a
// one-cycle lag (the periodic timer in SyncableStore handles steady-state).
QStringList RedisMdbRepository::getSetMembers(const QString &setKey)
{
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

QVariantList RedisMdbRepository::xrevrange(const QString &key, int count)
{
    if (m_worker) {
        auto *w = m_worker;
        QMetaObject::invokeMethod(w, [w, key, count]() {
            w->doXrevrange(key, count);
        }, Qt::QueuedConnection);
    }
    QMutexLocker lock(&m_resultMutex);
    return m_streamResults.value(key);
}

// Pub/sub (subscribe/unsubscribe)

void RedisMdbRepository::subscribe(const QString &channel, SubscriptionCallback callback)
{
    m_subscribers[channel].append(callback);

    // The channel name goes to the pub/sub thread; the callback stays here.
    // Subscribing before startWorker() is normal, and the worker replays the
    // whole set once it has a connection, so there is nothing to do yet.
    if (m_pubsub) {
        auto *p = m_pubsub;
        QMetaObject::invokeMethod(p, [p, channel]() { p->addChannel(channel); },
                                  Qt::QueuedConnection);
    }
}

void RedisMdbRepository::unsubscribe(const QString &channel)
{
    m_subscribers.remove(channel);

    if (m_pubsub) {
        auto *p = m_pubsub;
        QMetaObject::invokeMethod(p, [p, channel]() { p->removeChannel(channel); },
                                  Qt::QueuedConnection);
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

void RedisMdbRepository::onWorkerConnectionChanged(bool connected, bool usingBackup)
{
    bool wasConnected = m_connected;
    bool wasUsingBackup = m_usingBackup;
    m_connected = connected;
    m_usingBackup = usingBackup;

    if (connected)
        flushPendingWrites();

    if (connected && !wasConnected) {
        m_prolongedTimer->stop();
        if (m_prolongedDisconnect) {
            m_prolongedDisconnect = false;
            emit prolongedDisconnect(false);
        }
        if (usingBackup != wasUsingBackup)
            emit usingBackupConnection(usingBackup);
        emit connectionStateChanged(true);
        // Point pub/sub at whichever host the worker settled on. Its own
        // connection is not gated on this: it has been retrying since start,
        // so on a normal boot it is already up by the time this arrives.
        retargetPubsub();
    } else if (connected && usingBackup != wasUsingBackup) {
        // Worker switched hosts while staying connected (e.g. failback to primary)
        emit usingBackupConnection(usingBackup);
        retargetPubsub();
    } else if (!connected && wasConnected) {
        emit connectionStateChanged(false);
        m_prolongedTimer->start();
    }
}

// Pub/sub lives on its own thread; see PubsubWorker.

void RedisMdbRepository::retargetPubsub()
{
    if (!m_pubsub) return;
    const QString host = m_usingBackup ? m_backupHost : m_host;
    auto *p = m_pubsub;
    QMetaObject::invokeMethod(p, [p, host]() { p->setHost(host); }, Qt::QueuedConnection);
}

void RedisMdbRepository::dispatchPubsubMessage(const QString &channel, const QString &payload)
{
    auto it = m_subscribers.find(channel);
    if (it == m_subscribers.end()) return;
    for (const auto &cb : *it)
        cb(channel, payload);
}

// Re-read every subscribed hash. Called once a pub/sub connection is up:
// anything published while we had no subscription was missed, and without
// this the affected stores sit on stale values until their next poll -
// which for slow channels like battery:N is 30 seconds.
void RedisMdbRepository::refreshSubscribedChannels()
{
    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it)
        requestAll(it.key());
}
