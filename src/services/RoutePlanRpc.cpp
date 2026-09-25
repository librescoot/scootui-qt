#include "RoutePlanRpc.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

namespace {
constexpr int TimeoutMs = 3000;
const QString Channel = QStringLiteral("settings:route-plan");
}

RoutePlanRpc::RoutePlanRpc(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        finish({}, QStringLiteral("Route plan service timed out"));
    });
    connect(repo, &MdbRepository::subscriptionReady, this, [this](const QString &channel) {
        if (channel != m_replyChannel || m_queue.isEmpty() || m_inFlight) return;
        m_inFlight = true;
        const Request &request = m_queue.head();
        const QJsonObject envelope{
            {QStringLiteral("id"), m_replyChannel.mid(Channel.size() + 7)},
            {QStringLiteral("method"), request.method},
            {QStringLiteral("reply_channel"), m_replyChannel},
            {QStringLiteral("deadline"), double(QDateTime::currentMSecsSinceEpoch() + TimeoutMs)},
            {QStringLiteral("payload"), request.payload}
        };
        m_repo->push(Channel, QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    });
    connect(repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        if (!connected && !m_queue.isEmpty())
            finish({}, QStringLiteral("Route plan service disconnected"));
    });
}

RoutePlanRpc::~RoutePlanRpc()
{
    if (m_subscription)
        m_repo->unsubscribe(m_replyChannel, m_subscription);
}

void RoutePlanRpc::call(const QString &method, const QJsonObject &payload, Completion completion)
{
    m_queue.enqueue({method, payload, std::move(completion)});
    sendNext();
}

void RoutePlanRpc::sendNext()
{
    if (m_subscription || m_queue.isEmpty()) return;
    m_replyChannel = Channel + QStringLiteral(":reply:")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_inFlight = false;
    m_timeout.start(TimeoutMs);
    m_subscription = m_repo->subscribe(m_replyChannel, [this](const QString &, const QString &data) {
        if (!m_inFlight) return;
        QJsonParseError parse;
        const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parse);
        if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
            finish({}, QStringLiteral("Invalid route plan reply"));
            return;
        }
        const QJsonObject reply = doc.object();
        finish(reply.value(QStringLiteral("payload")).toObject(),
               reply.value(QStringLiteral("ok")).toBool()
                   ? QString() : reply.value(QStringLiteral("error")).toString(
                         QStringLiteral("Route plan request rejected")));
    });
}

void RoutePlanRpc::finish(const QJsonObject &plan, const QString &error)
{
    m_timeout.stop();
    m_repo->unsubscribe(m_replyChannel, m_subscription);
    m_subscription = 0;
    m_inFlight = false;
    const Request request = m_queue.dequeue();
    request.completion(plan, error);
    QTimer::singleShot(0, this, [this]() { sendNext(); });
}
