#include "DestinationRpc.h"
#include "repositories/MdbRepository.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

DestinationRpc::DestinationRpc(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
    // The reply lands on whatever thread delivers subscription messages; the
    // lambda queues back onto this object's thread when they differ.
    connect(this, &DestinationRpc::replyArrived, this,
            [this](bool ok, const QVariantMap &payload, const QString &) {
                if (!m_waiting)
                    return;
                m_gotReply = true;
                m_replyOk = ok;
                m_payload = payload;
                if (m_loop)
                    m_loop->quit();
            });
    if (m_repo) {
        m_subscriptionId = m_repo->subscribe(
            QLatin1String(ReplyChannel),
            [this](const QString &, const QString &message) { handleMessage(message); });
    }
}

DestinationRpc::~DestinationRpc()
{
    if (m_repo && m_subscriptionId != 0)
        m_repo->unsubscribe(QLatin1String(ReplyChannel), m_subscriptionId);
}

bool DestinationRpc::call(const QString &method, const QVariantMap &args, QVariantMap *reply,
                          int timeoutMs)
{
    if (!m_repo || m_waiting)
        return false;

    QJsonObject envelope;
    envelope.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    envelope.insert(QStringLiteral("method"), method);
    envelope.insert(QStringLiteral("reply_channel"), QLatin1String(ReplyChannel));
    envelope.insert(QStringLiteral("deadline"),
                    double(QDateTime::currentMSecsSinceEpoch() + timeoutMs));
    envelope.insert(QStringLiteral("payload"), QJsonObject::fromVariantMap(args));

    m_waiting = true;
    m_gotReply = false;
    m_payload.clear();

    QEventLoop loop;
    m_loop = &loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    m_repo->push(QLatin1String(RequestChannel),
                 QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    // An in-process server answers inside push(); only a remote one needs the
    // loop to wait for the reply.
    if (!m_gotReply)
        loop.exec();

    m_loop = nullptr;
    m_waiting = false;
    if (!m_gotReply)
        return false;
    if (reply)
        *reply = m_payload;
    return m_replyOk;
}

void DestinationRpc::handleMessage(const QString &message)
{
    const QJsonObject reply = QJsonDocument::fromJson(message.toUtf8()).object();
    emit replyArrived(reply.value(QStringLiteral("ok")).toBool(),
                      reply.value(QStringLiteral("payload")).toObject().toVariantMap(),
                      reply.value(QStringLiteral("error")).toString());
}
