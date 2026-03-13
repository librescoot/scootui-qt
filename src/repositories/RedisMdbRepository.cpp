#include "RedisMdbRepository.h"
#include <QDebug>
#include <QCoreApplication>

// ============================================================
// RedisConnection — synchronous RESP client over QTcpSocket
// ============================================================

RedisConnection::RedisConnection(const QString &host, quint16 port, QObject *parent)
    : QObject(parent), m_host(host), m_port(port)
{
    m_socket = new QTcpSocket(this);
}

RedisConnection::~RedisConnection()
{
    disconnect();
}

bool RedisConnection::connectToServer(int timeoutMs)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        return true;

    m_socket->connectToHost(m_host, m_port);
    if (!m_socket->waitForConnected(timeoutMs)) {
        return false;
    }
    m_buffer.clear();
    return true;
}

void RedisConnection::disconnect()
{
    m_socket->disconnectFromHost();
}

bool RedisConnection::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QByteArray RedisConnection::buildCommand(const QStringList &args)
{
    QByteArray cmd;
    cmd.append('*');
    cmd.append(QByteArray::number(args.size()));
    cmd.append("\r\n");
    for (const auto &arg : args) {
        QByteArray utf8 = arg.toUtf8();
        cmd.append('$');
        cmd.append(QByteArray::number(utf8.size()));
        cmd.append("\r\n");
        cmd.append(utf8);
        cmd.append("\r\n");
    }
    return cmd;
}

bool RedisConnection::readLine(QByteArray &line, int timeoutMs)
{
    while (true) {
        int idx = m_buffer.indexOf("\r\n");
        if (idx >= 0) {
            line = m_buffer.left(idx);
            m_buffer.remove(0, idx + 2);
            return true;
        }
        if (!m_socket->waitForReadyRead(timeoutMs))
            return false;
        m_buffer.append(m_socket->readAll());
    }
}

bool RedisConnection::readBytes(int count, QByteArray &data, int timeoutMs)
{
    // Need count + 2 bytes (data + \r\n)
    int need = count + 2;
    while (m_buffer.size() < need) {
        if (!m_socket->waitForReadyRead(timeoutMs))
            return false;
        m_buffer.append(m_socket->readAll());
    }
    data = m_buffer.left(count);
    m_buffer.remove(0, need);
    return true;
}

QVariant RedisConnection::parseReply()
{
    QByteArray line;
    if (!readLine(line, 2000))
        return QVariant();

    char type = line.at(0);
    QByteArray payload = line.mid(1);

    switch (type) {
    case '+': // Simple string
        return QString::fromUtf8(payload);
    case '-': // Error
        qWarning() << "Redis error:" << payload;
        return QVariant();
    case ':': // Integer
        return payload.toLongLong();
    case '$': { // Bulk string
        int len = payload.toInt();
        if (len == -1)
            return QVariant(); // null
        QByteArray data;
        if (!readBytes(len, data, 2000))
            return QVariant();
        return QString::fromUtf8(data);
    }
    case '*': { // Array
        int count = payload.toInt();
        if (count == -1)
            return QVariant();
        QVariantList list;
        list.reserve(count);
        for (int i = 0; i < count; ++i) {
            list.append(parseReply());
        }
        return list;
    }
    default:
        qWarning() << "Unknown RESP type:" << type;
        return QVariant();
    }
}

QVariant RedisConnection::command(const QStringList &args, int timeoutMs)
{
    if (!isConnected())
        return QVariant();

    QByteArray cmd = buildCommand(args);
    m_socket->write(cmd);
    if (!m_socket->waitForBytesWritten(timeoutMs))
        return QVariant();

    return parseReply();
}

QString RedisConnection::hget(const QString &key, const QString &field)
{
    return command({QStringLiteral("HGET"), key, field}).toString();
}

FieldMap RedisConnection::hgetall(const QString &key)
{
    FieldMap result;
    QVariant reply = command({QStringLiteral("HGETALL"), key});
    QVariantList list = reply.toList();
    for (int i = 0; i + 1 < list.size(); i += 2) {
        result.insert(list[i].toString(), list[i + 1].toString());
    }
    return result;
}

void RedisConnection::hset(const QString &key, const QString &field, const QString &value)
{
    command({QStringLiteral("HSET"), key, field, value});
}

void RedisConnection::hdel(const QString &key, const QString &field)
{
    command({QStringLiteral("HDEL"), key, field});
}

void RedisConnection::publish(const QString &channel, const QString &message)
{
    command({QStringLiteral("PUBLISH"), channel, message});
}

void RedisConnection::lpush(const QString &key, const QString &value)
{
    command({QStringLiteral("LPUSH"), key, value});
}

QStringList RedisConnection::smembers(const QString &key)
{
    QStringList result;
    QVariant reply = command({QStringLiteral("SMEMBERS"), key});
    for (const auto &v : reply.toList())
        result.append(v.toString());
    return result;
}

QStringList RedisConnection::lrange(const QString &key, int start, int stop)
{
    QStringList result;
    QVariant reply = command({QStringLiteral("LRANGE"), key,
                              QString::number(start), QString::number(stop)});
    for (const auto &v : reply.toList())
        result.append(v.toString());
    return result;
}

void RedisConnection::sadd(const QString &key, const QString &member)
{
    command({QStringLiteral("SADD"), key, member});
}

void RedisConnection::srem(const QString &key, const QString &member)
{
    command({QStringLiteral("SREM"), key, member});
}

// ============================================================
// RedisPubsubWorker — dedicated thread for SUBSCRIBE
// ============================================================

RedisPubsubWorker::RedisPubsubWorker(const QString &host, quint16 port)
    : m_host(host), m_port(port)
{
}

RedisPubsubWorker::~RedisPubsubWorker()
{
    stop();
}

void RedisPubsubWorker::start()
{
    m_running = true;
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &RedisPubsubWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &RedisPubsubWorker::onDisconnected);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &RedisPubsubWorker::tryReconnect);

    tryReconnect();
}

void RedisPubsubWorker::stop()
{
    m_running = false;
    if (m_reconnectTimer)
        m_reconnectTimer->stop();
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

void RedisPubsubWorker::subscribe(const QString &channel)
{
    if (!m_channels.contains(channel))
        m_channels.append(channel);

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray cmd;
        cmd.append("*2\r\n$9\r\nSUBSCRIBE\r\n$");
        QByteArray ch = channel.toUtf8();
        cmd.append(QByteArray::number(ch.size()));
        cmd.append("\r\n");
        cmd.append(ch);
        cmd.append("\r\n");
        m_socket->write(cmd);
    }
}

void RedisPubsubWorker::unsubscribe(const QString &channel)
{
    m_channels.removeAll(channel);

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray cmd;
        cmd.append("*2\r\n$11\r\nUNSUBSCRIBE\r\n$");
        QByteArray ch = channel.toUtf8();
        cmd.append(QByteArray::number(ch.size()));
        cmd.append("\r\n");
        cmd.append(ch);
        cmd.append("\r\n");
        m_socket->write(cmd);
    }
}

void RedisPubsubWorker::tryReconnect()
{
    if (!m_running) return;

    m_socket->connectToHost(m_host, m_port);
    if (m_socket->waitForConnected(2000)) {
        m_buffer.clear();
        // Resubscribe to all channels
        for (const auto &ch : m_channels) {
            QByteArray cmd;
            cmd.append("*2\r\n$9\r\nSUBSCRIBE\r\n$");
            QByteArray chUtf8 = ch.toUtf8();
            cmd.append(QByteArray::number(chUtf8.size()));
            cmd.append("\r\n");
            cmd.append(chUtf8);
            cmd.append("\r\n");
            m_socket->write(cmd);
        }
        emit connected();
    } else {
        emit disconnected();
        if (m_running)
            m_reconnectTimer->start(2000);
    }
}

void RedisPubsubWorker::onDisconnected()
{
    emit disconnected();
    if (m_running)
        m_reconnectTimer->start(1000);
}

QVariant RedisPubsubWorker::parseReply()
{
    if (m_buffer.isEmpty()) return QVariant();

    // Check we have a complete line for the type indicator
    int idx = m_buffer.indexOf("\r\n");
    if (idx < 0) return QVariant();

    char type = m_buffer.at(0);
    QByteArray payload = m_buffer.mid(1, idx - 1);

    switch (type) {
    case '+':
        m_buffer.remove(0, idx + 2);
        return QString::fromUtf8(payload);
    case '-':
        m_buffer.remove(0, idx + 2);
        return QVariant();
    case ':':
        m_buffer.remove(0, idx + 2);
        return payload.toLongLong();
    case '$': {
        int len = payload.toInt();
        if (len == -1) {
            m_buffer.remove(0, idx + 2);
            return QVariant();
        }
        int dataStart = idx + 2;
        int need = dataStart + len + 2;
        if (m_buffer.size() < need)
            return QVariant(); // incomplete
        QString result = QString::fromUtf8(m_buffer.mid(dataStart, len));
        m_buffer.remove(0, need);
        return result;
    }
    case '*': {
        int count = payload.toInt();
        if (count == -1) {
            m_buffer.remove(0, idx + 2);
            return QVariant();
        }
        // Save buffer state in case array is incomplete
        QByteArray saved = m_buffer;
        m_buffer.remove(0, idx + 2);
        QVariantList list;
        for (int i = 0; i < count; ++i) {
            QVariant elem = parseReply();
            if (!elem.isValid()) {
                // Incomplete — restore buffer
                m_buffer = saved;
                return QVariant();
            }
            list.append(elem);
        }
        return list;
    }
    default:
        return QVariant();
    }
}

void RedisPubsubWorker::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (!m_buffer.isEmpty()) {
        QByteArray saved = m_buffer;
        QVariant reply = parseReply();
        if (!reply.isValid()) {
            m_buffer = saved;
            break;
        }

        QVariantList list = reply.toList();
        if (list.size() >= 3) {
            QString type = list[0].toString();
            if (type == QLatin1String("message")) {
                emit messageReceived(list[1].toString(), list[2].toString());
            }
        }
    }
}

// ============================================================
// RedisMdbRepository — main repository implementation
// ============================================================

RedisMdbRepository::RedisMdbRepository(const QString &host, quint16 port, QObject *parent)
    : MdbRepository(parent), m_host(host), m_port(port)
{
    m_conn = new RedisConnection(host, port, this);

    // Pubsub worker on dedicated thread
    m_pubsubThread = new QThread(this);
    m_pubsubWorker = new RedisPubsubWorker(host, port);
    m_pubsubWorker->moveToThread(m_pubsubThread);

    connect(m_pubsubThread, &QThread::started, m_pubsubWorker, &RedisPubsubWorker::start);
    connect(m_pubsubWorker, &RedisPubsubWorker::messageReceived,
            this, &RedisMdbRepository::onPubsubMessage, Qt::QueuedConnection);
    connect(m_pubsubWorker, &RedisPubsubWorker::connected,
            this, &RedisMdbRepository::onPubsubConnected, Qt::QueuedConnection);
    connect(m_pubsubWorker, &RedisPubsubWorker::disconnected,
            this, &RedisMdbRepository::onPubsubDisconnected, Qt::QueuedConnection);

    // Reconnect timer for the main connection
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        ensureConnected();
    });

    // Prolonged disconnect timer (5 seconds)
    m_prolongedTimer = new QTimer(this);
    m_prolongedTimer->setSingleShot(true);
    m_prolongedTimer->setInterval(5000);
    connect(m_prolongedTimer, &QTimer::timeout, this, [this]() {
        if (!m_connected && !m_prolongedDisconnect) {
            m_prolongedDisconnect = true;
            emit prolongedDisconnect(true);
        }
    });

    // Initial connection attempt
    if (ensureConnected()) {
        qDebug() << "Redis connected to" << host << ":" << port;
    } else {
        qWarning() << "Redis initial connection failed, will retry...";
        m_prolongedTimer->start();
        m_reconnectTimer->start(1000);
    }

    m_pubsubThread->start();
}

RedisMdbRepository::~RedisMdbRepository()
{
    QMetaObject::invokeMethod(m_pubsubWorker, &RedisPubsubWorker::stop, Qt::BlockingQueuedConnection);
    m_pubsubThread->quit();
    m_pubsubThread->wait(2000);
    delete m_pubsubWorker;
}

bool RedisMdbRepository::ensureConnected()
{
    if (m_conn->isConnected())
        return true;

    bool ok = m_conn->connectToServer(2000);
    if (ok && !m_connected) {
        m_connected = true;
        m_reconnectTimer->stop();
        if (m_prolongedDisconnect) {
            m_prolongedDisconnect = false;
            emit prolongedDisconnect(false);
        }
        emit connectionStateChanged(true);
    } else if (!ok && m_connected) {
        m_connected = false;
        emit connectionStateChanged(false);
        m_prolongedTimer->start();
        m_reconnectTimer->start(1000);
    } else if (!ok) {
        m_reconnectTimer->start(2000);
    }
    return ok;
}

void RedisMdbRepository::startReconnectTimer()
{
    if (!m_reconnectTimer->isActive())
        m_reconnectTimer->start(1000);
}

void RedisMdbRepository::onPubsubMessage(const QString &channel, const QString &message)
{
    auto it = m_subscribers.find(channel);
    if (it != m_subscribers.end()) {
        for (const auto &cb : *it)
            cb(channel, message);
    }
}

void RedisMdbRepository::onPubsubConnected()
{
    if (!m_connected) {
        ensureConnected();
    }
}

void RedisMdbRepository::onPubsubDisconnected()
{
    if (m_connected) {
        m_connected = false;
        emit connectionStateChanged(false);
        m_prolongedTimer->start();
    }
}

QString RedisMdbRepository::get(const QString &channel, const QString &variable)
{
    if (!ensureConnected()) return {};
    QString result = m_conn->hget(channel, variable);
    if (!m_conn->isConnected()) {
        m_connected = false;
        emit connectionStateChanged(false);
        startReconnectTimer();
    }
    return result;
}

FieldMap RedisMdbRepository::getAll(const QString &channel)
{
    if (!ensureConnected()) return {};
    FieldMap result = m_conn->hgetall(channel);
    if (!m_conn->isConnected()) {
        m_connected = false;
        emit connectionStateChanged(false);
        startReconnectTimer();
    }
    return result;
}

void RedisMdbRepository::set(const QString &channel, const QString &variable,
                              const QString &value, bool doPublish)
{
    if (!ensureConnected()) return;
    m_conn->hset(channel, variable, value);
    if (doPublish)
        m_conn->publish(channel, variable);
}

void RedisMdbRepository::publish(const QString &channel, const QString &message)
{
    if (!ensureConnected()) return;
    m_conn->publish(channel, message);
}

void RedisMdbRepository::subscribe(const QString &channel, SubscriptionCallback callback)
{
    m_subscribers[channel].append(callback);
    QMetaObject::invokeMethod(m_pubsubWorker, [this, channel]() {
        m_pubsubWorker->subscribe(channel);
    }, Qt::QueuedConnection);
}

void RedisMdbRepository::unsubscribe(const QString &channel)
{
    m_subscribers.remove(channel);
    QMetaObject::invokeMethod(m_pubsubWorker, [this, channel]() {
        m_pubsubWorker->unsubscribe(channel);
    }, Qt::QueuedConnection);
}

void RedisMdbRepository::push(const QString &channel, const QString &command)
{
    if (!ensureConnected()) return;
    m_conn->lpush(channel, command);
}

void RedisMdbRepository::dashboardReady()
{
    if (!ensureConnected()) return;
    set(QStringLiteral("dashboard"), QStringLiteral("ready"), QStringLiteral("true"));
}

void RedisMdbRepository::publishButtonEvent(const QString &event)
{
    publish(QStringLiteral("dashboard"), event);
}

QStringList RedisMdbRepository::getSetMembers(const QString &setKey)
{
    if (!ensureConnected()) return {};
    return m_conn->smembers(setKey);
}

void RedisMdbRepository::addToSet(const QString &setKey, const QString &member)
{
    if (!ensureConnected()) return;
    m_conn->sadd(setKey, member);
}

void RedisMdbRepository::removeFromSet(const QString &setKey, const QString &member)
{
    if (!ensureConnected()) return;
    m_conn->srem(setKey, member);
}

void RedisMdbRepository::hdel(const QString &key, const QString &field)
{
    if (!ensureConnected()) return;
    m_conn->hdel(key, field);
}

QStringList RedisMdbRepository::lrange(const QString &key, int start, int stop)
{
    if (!ensureConnected()) return {};
    return m_conn->lrange(key, start, stop);
}
