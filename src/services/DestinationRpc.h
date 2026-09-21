#pragma once

#include <QObject>
#include <QVariantMap>
#include "repositories/MdbRepository.h"

class MdbRepository;
class QEventLoop;

// Client for settings-service's destination management RPC. Calls carry a
// JSON envelope on the request queue and wait for a reply on the shared reply
// channel; one call is in flight at a time, so every reply that arrives while
// a call waits belongs to it.
class DestinationRpc : public QObject
{
    Q_OBJECT

public:
    static constexpr char RequestChannel[] = "settings:destinations";
    static constexpr char ReplyChannel[] = "settings:destinations:reply";

    explicit DestinationRpc(MdbRepository *repo, QObject *parent = nullptr);
    ~DestinationRpc() override;

    // Blocks the calling thread until the reply or the timeout. Returns false
    // on timeout, transport failure, or a server-side error; on success the
    // reply payload is written to *reply.
    bool call(const QString &method, const QVariantMap &args, QVariantMap *reply,
              int timeoutMs = 3000);

signals:
    void replyArrived(bool ok, const QVariantMap &payload, const QString &error);

private:
    void handleMessage(const QString &message);

    MdbRepository *m_repo;
    SubscriptionId m_subscriptionId = 0;
    QEventLoop *m_loop = nullptr;
    bool m_waiting = false;
    bool m_gotReply = false;
    bool m_replyOk = false;
    QVariantMap m_payload;
};
