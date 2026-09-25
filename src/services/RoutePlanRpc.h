#pragma once

#include "repositories/MdbRepository.h"

#include <QJsonObject>
#include <QQueue>
#include <QTimer>
#include <functional>

// Serialized, nonblocking redis-ipc CallMethod client. The reply subscription
// must be acknowledged before a request is pushed, or a fast reply can be lost.
class RoutePlanRpc : public QObject
{
    Q_OBJECT
public:
    using Completion = std::function<void(const QJsonObject &, const QString &)>;
    explicit RoutePlanRpc(MdbRepository *repo, QObject *parent = nullptr);
    ~RoutePlanRpc() override;
    void call(const QString &method, const QJsonObject &payload, Completion completion);

private:
    struct Request { QString method; QJsonObject payload; Completion completion; };
    void sendNext();
    void finish(const QJsonObject &plan, const QString &error);
    MdbRepository *m_repo;
    QString m_replyChannel;
    SubscriptionId m_subscription = 0;
    QQueue<Request> m_queue;
    QTimer m_timeout;
    bool m_inFlight = false;
};
