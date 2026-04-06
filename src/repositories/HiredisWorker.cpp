#include "HiredisWorker.h"
#include <QDebug>
#include <numeric>
#include <hiredis/hiredis.h>

HiredisWorker::HiredisWorker(const QString &host, quint16 port,
                             const QString &backupHost)
    : m_host(host), m_port(port), m_backupHost(backupHost)
{
}

HiredisWorker::~HiredisWorker()
{
    stop();
}

void HiredisWorker::registerChannel(const QString &channel, int intervalMs)
{
    for (auto &ch : m_channels) {
        if (ch.channel == channel) {
            ch.intervalMs = qMin(ch.intervalMs, intervalMs);
            return;
        }
    }
    m_channels.append({channel, intervalMs, 0});
}

// Compute GCD of all intervals and set up tick modulos
static int computeGcd(int a, int b)
{
    while (b) { int t = b; b = a % b; a = t; }
    return a;
}

void HiredisWorker::start()
{
    m_running = true;

    // Compute GCD-based poll interval
    if (m_channels.isEmpty()) {
        qWarning() << "HiredisWorker: no channels registered";
        return;
    }

    // Create timers first (ensureConnected may need them)
    m_pollTimer = new QTimer(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(2000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &HiredisWorker::tryReconnect);
    m_primaryProbeTimer = new QTimer(this);
    m_primaryProbeTimer->setInterval(2000);
    connect(m_primaryProbeTimer, &QTimer::timeout, this, [this]() {
        if (!m_usingBackup || !m_connected) return;

        // Try switching back to primary
        redisContext *probe = redisConnectWithTimeout(
            m_host.toUtf8().constData(), m_port, {0, 200000});
        if (probe && !probe->err) {
            redisFree(probe);
            // Primary is back, switch
            disconnectRedis();
            if (ensureConnected()) {
                qDebug() << "HiredisWorker: switched back to primary" << m_host;
                m_primaryProbeTimer->stop();
            }
        } else {
            if (probe) redisFree(probe);
        }
    });

    // Compute GCD-based poll interval (clamp first, then compute modulos)
    m_gcdInterval = m_channels.first().intervalMs;
    for (int i = 1; i < m_channels.size(); ++i)
        m_gcdInterval = computeGcd(m_gcdInterval, m_channels[i].intervalMs);
    m_gcdInterval = qMax(m_gcdInterval, 100);

    for (auto &ch : m_channels)
        ch.tickModulo = qMax(1, ch.intervalMs / m_gcdInterval);

    m_pollTimer->setInterval(m_gcdInterval);
    connect(m_pollTimer, &QTimer::timeout, this, &HiredisWorker::onPollTimer);

    qDebug() << "HiredisWorker: GCD interval" << m_gcdInterval << "ms,"
             << m_channels.size() << "channels";

    // Connect to Redis
    if (!ensureConnected()) {
        qWarning() << "HiredisWorker: initial connection failed, will retry";
    }

    // Initial full fetch + start polling
    if (m_connected) {
        fetchAll();
        m_pollTimer->start();
    }
}

void HiredisWorker::stop()
{
    m_running = false;
    if (m_pollTimer) m_pollTimer->stop();
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_primaryProbeTimer) m_primaryProbeTimer->stop();
    disconnectRedis();
}

bool HiredisWorker::ensureConnected()
{
    if (m_ctx && !m_ctx->err) {
        if (!m_connected) {
            m_connected = true;
            emit connectionChanged(true, m_usingBackup);
        }
        return true;
    }

    disconnectRedis();

    // Try primary (200ms connect timeout for USB)
    struct timeval tv = {0, 200000};
    m_ctx = redisConnectWithTimeout(m_host.toUtf8().constData(), m_port, tv);
    if (m_ctx && !m_ctx->err) {
        // Set socket timeout so commands don't block forever on a half-open connection
        struct timeval cmdTimeout = {2, 0};
        redisSetTimeout(m_ctx, cmdTimeout);
        m_connected = true;
        m_usingBackup = false;
        emit connectionChanged(true, false);
        return true;
    }
    if (m_ctx) { redisFree(m_ctx); m_ctx = nullptr; }

    // Try backup (1s connect timeout)
    if (!m_backupHost.isEmpty()) {
        tv = {1, 0};
        m_ctx = redisConnectWithTimeout(m_backupHost.toUtf8().constData(), m_port, tv);
        if (m_ctx && !m_ctx->err) {
            struct timeval cmdTimeout = {2, 0};
            redisSetTimeout(m_ctx, cmdTimeout);
            m_connected = true;
            m_usingBackup = true;
            m_primaryProbeTimer->start();
            qDebug() << "HiredisWorker: connected to backup" << m_backupHost;
            emit connectionChanged(true, true);
            return true;
        }
        if (m_ctx) { redisFree(m_ctx); m_ctx = nullptr; }
    }

    if (m_connected) {
        m_connected = false;
        emit connectionChanged(false, m_usingBackup);
    }

    if (m_running && !m_reconnectTimer->isActive())
        m_reconnectTimer->start();

    return false;
}

void HiredisWorker::disconnectRedis()
{
    if (m_ctx) {
        redisFree(m_ctx);
        m_ctx = nullptr;
    }
}

void HiredisWorker::tryReconnect()
{
    if (!m_running) return;

    if (ensureConnected()) {
        fetchAll();
        m_pollTimer->start();
    } else {
        m_reconnectTimer->start();
    }
}

void HiredisWorker::fetchAll()
{
    if (!m_connected) return;

    for (const auto &ch : m_channels)
        pollChannel(ch.channel);
}

void HiredisWorker::onPollTimer()
{
    if (!ensureConnected()) {
        m_pollTimer->stop();
        m_reconnectTimer->start();
        return;
    }

    m_tickCount++;
    for (const auto &ch : m_channels) {
        if (ch.tickModulo > 0 && (m_tickCount % ch.tickModulo) == 0)
            pollChannel(ch.channel);
    }
}

void HiredisWorker::pollChannel(const QString &channel)
{
    if (!m_ctx) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "HGETALL %s", channel.toUtf8().constData()));

    if (!reply) {
        qWarning() << "HiredisWorker: HGETALL" << channel << "failed:" << m_ctx->errstr;
        disconnectRedis();
        m_connected = false;
        emit connectionChanged(false, m_usingBackup);
        return;
    }

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
        FieldMap fields;
        fields.reserve(static_cast<int>(reply->elements / 2));
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            fields.insert(
                QString::fromUtf8(reply->element[i]->str, reply->element[i]->len),
                QString::fromUtf8(reply->element[i + 1]->str, reply->element[i + 1]->len));
        }
        emit fieldsUpdated(channel, fields);
    }

    freeReplyObject(reply);
}

void HiredisWorker::doHget(const QString &channel, const QString &field)
{
    if (!ensureConnected()) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "HGET %s %s",
                     channel.toUtf8().constData(),
                     field.toUtf8().constData()));
    if (!reply) {
        qWarning() << "HiredisWorker: HGET" << channel << field << "failed:" << m_ctx->errstr;
        disconnectRedis();
        m_connected = false;
        emit connectionChanged(false, m_usingBackup);
        return;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        emit fieldFetched(channel, field, QString::fromUtf8(reply->str, reply->len));
    }
    freeReplyObject(reply);
}

// Write operations

void HiredisWorker::doSet(const QString &channel, const QString &variable,
                          const QString &value, bool publish)
{
    if (!ensureConnected()) {
        qWarning() << "HiredisWorker: HSET failed (not connected):" << channel << variable;
        return;
    }

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "HSET %s %s %s",
                     channel.toUtf8().constData(),
                     variable.toUtf8().constData(),
                     value.toUtf8().constData()));
    if (!reply) {
        qWarning() << "HiredisWorker: HSET" << channel << variable << "error:" << m_ctx->errstr;
        disconnectRedis();
        m_connected = false;
        emit connectionChanged(false, m_usingBackup);
        return;
    }
    freeReplyObject(reply);

    if (publish) {
        reply = static_cast<redisReply *>(
            redisCommand(m_ctx, "PUBLISH %s %s",
                         channel.toUtf8().constData(),
                         variable.toUtf8().constData()));
        if (reply) freeReplyObject(reply);
    }
}

void HiredisWorker::doPush(const QString &channel, const QString &command)
{
    if (!ensureConnected()) {
        qWarning() << "HiredisWorker: LPUSH failed (not connected):" << channel << command;
        return;
    }

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "LPUSH %s %s",
                     channel.toUtf8().constData(),
                     command.toUtf8().constData()));
    if (!reply) {
        qWarning() << "HiredisWorker: LPUSH" << channel << command << "error:" << m_ctx->errstr;
        disconnectRedis();
        m_connected = false;
        emit connectionChanged(false, m_usingBackup);
        return;
    }
    freeReplyObject(reply);
}

void HiredisWorker::doPublish(const QString &channel, const QString &message)
{
    if (!ensureConnected()) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "PUBLISH %s %s",
                     channel.toUtf8().constData(),
                     message.toUtf8().constData()));
    if (reply) freeReplyObject(reply);
}

void HiredisWorker::doHdel(const QString &key, const QString &field)
{
    if (!ensureConnected()) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "HDEL %s %s",
                     key.toUtf8().constData(),
                     field.toUtf8().constData()));
    if (reply) freeReplyObject(reply);
}

void HiredisWorker::doAddToSet(const QString &setKey, const QString &member)
{
    if (!ensureConnected()) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "SADD %s %s",
                     setKey.toUtf8().constData(),
                     member.toUtf8().constData()));
    if (reply) freeReplyObject(reply);
}

void HiredisWorker::doRemoveFromSet(const QString &setKey, const QString &member)
{
    if (!ensureConnected()) return;

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "SREM %s %s",
                     setKey.toUtf8().constData(),
                     member.toUtf8().constData()));
    if (reply) freeReplyObject(reply);
}

void HiredisWorker::doGetSetMembers(const QString &setKey)
{
    QStringList members;
    if (!ensureConnected()) {
        emit setMembersResult(setKey, members);
        return;
    }

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "SMEMBERS %s", setKey.toUtf8().constData()));
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i)
            members.append(QString::fromUtf8(reply->element[i]->str, reply->element[i]->len));
    }
    if (reply) freeReplyObject(reply);
    emit setMembersResult(setKey, members);
}

void HiredisWorker::doLrange(const QString &key, int start, int stop)
{
    QStringList values;
    if (!ensureConnected()) {
        emit lrangeResult(key, values);
        return;
    }

    redisReply *reply = static_cast<redisReply *>(
        redisCommand(m_ctx, "LRANGE %s %d %d",
                     key.toUtf8().constData(), start, stop));
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i)
            values.append(QString::fromUtf8(reply->element[i]->str, reply->element[i]->len));
    }
    if (reply) freeReplyObject(reply);
    emit lrangeResult(key, values);
}
