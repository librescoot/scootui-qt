#include "CloudPairingService.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

namespace {
const QString Channel = QStringLiteral("cloud:pairing");
constexpr int TimeoutMs = 10000;
}

CloudPairingService::CloudPairingService(MdbRepository *repo, QObject *parent)
    : QObject(parent), m_repo(repo)
{
    m_poll.setInterval(2000);
    m_expiry.setInterval(1000);
    m_timeout.setSingleShot(true);
    connect(&m_poll, &QTimer::timeout, this, &CloudPairingService::refresh);
    connect(&m_timeout, &QTimer::timeout, this, &CloudPairingService::unavailable);
    connect(&m_expiry, &QTimer::timeout, this, [this]() {
        if (m_expiresAt && m_expiresAt <= QDateTime::currentSecsSinceEpoch()) {
            m_expired = true;
            m_code.clear();
            m_expiresAt = 0;
            emit changed();
        }
    });
    connect(repo, &MdbRepository::subscriptionReady, this, [this](const QString &channel) {
        if (!m_busy || m_sent || channel != m_channel) return;
        if (QDateTime::currentMSecsSinceEpoch() >= m_deadline) { unavailable(); return; }
        m_sent = true;
        QJsonObject envelope{
            {QStringLiteral("id"), m_channel.mid(Channel.size() + 7)},
            {QStringLiteral("method"), m_action},
            {QStringLiteral("reply_channel"), m_channel},
            {QStringLiteral("deadline"), double(m_deadline)},
            {QStringLiteral("payload"), QJsonObject{}}
        };
        m_repo->push(Channel, QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    });
    connect(repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        if (!connected) unavailable();
        else if (m_visible) refresh();
        emit changed();
    });
}

CloudPairingService::~CloudPairingService() { clearRequest(); }

QString CloudPairingService::code() const
{
    return m_expiresAt > QDateTime::currentSecsSinceEpoch() ? m_code : QString();
}

bool CloudPairingService::online() const
{
    return m_internetAvailable && m_repo->isConnected();
}

void CloudPairingService::setInternetAvailable(bool available)
{
    if (m_internetAvailable == available) return;
    m_internetAvailable = available;
    if (!available) {
        m_code.clear();
        m_expiresAt = 0;
    }
    if (available && m_visible) refresh();
    emit changed();
}

bool CloudPairingService::canStart() const
{
    return m_visible && m_stationary && online() && !busy() && !m_running
        && m_state != QLatin1String("connected") && m_state != QLatin1String("unavailable");
}

bool CloudPairingService::canStop() const
{
    return m_visible && online() && !busy() && m_running
        && m_state != QLatin1String("stopping");
}

void CloudPairingService::setStationary(bool stationary)
{
    if (m_stationary == stationary) return;
    m_stationary = stationary;
    emit changed();
}

void CloudPairingService::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    if (visible) {
        m_poll.start();
        m_expiry.start();
        refresh();
    } else {
        m_poll.stop();
        m_expiry.stop();
        if (!busy()) clearRequest();
        m_code.clear();
        m_expiresAt = 0;
    }
    emit changed();
}

void CloudPairingService::start() { if (canStart()) { clearRequest(); request(QStringLiteral("start")); } }
void CloudPairingService::stop() { if (canStop()) { clearRequest(); request(QStringLiteral("stop")); } }
void CloudPairingService::refresh() { if (m_visible && !m_busy) request(QStringLiteral("status")); }
void CloudPairingService::retry()
{
    if (!m_visible || !online() || busy()) return;
    clearRequest();
    m_foregroundStatus = true;
    request(QStringLiteral("status"));
}

void CloudPairingService::request(const QString &action)
{
    if (!m_repo->isConnected()) { unavailable(); return; }
    m_busy = true;
    m_sent = false;
    m_action = action;
    if (action == QLatin1String("start")) {
        m_state = QStringLiteral("starting");
        m_expired = false;
        m_code.clear();
        m_expiresAt = 0;
    }
    if (action == QLatin1String("stop")) {
        m_state = QStringLiteral("stopping");
        m_code.clear();
        m_expiresAt = 0;
    }
    m_channel = Channel + QStringLiteral(":reply:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_deadline = QDateTime::currentMSecsSinceEpoch() + TimeoutMs;
    m_timeout.start(TimeoutMs);
    // Publish only after SUBSCRIBE acknowledgment, never optimistically.
    m_subscription = m_repo->subscribe(m_channel, [this](const QString &channel, const QString &data) { receive(channel, data); });
    emit changed();
}

void CloudPairingService::clearRequest()
{
    m_timeout.stop();
    if (m_subscription) m_repo->unsubscribe(m_channel, m_subscription);
    m_subscription = 0;
    m_busy = m_sent = false;
}

void CloudPairingService::unavailable()
{
    clearRequest();
    m_state = QStringLiteral("unavailable");
    m_foregroundStatus = false;
    m_code.clear();
    m_expiresAt = 0;
    m_running = false;
    emit changed();
}

void CloudPairingService::receive(const QString &channel, const QString &data)
{
    if (!m_busy || !m_sent || channel != m_channel) return;
    if (data.toUtf8().size() > 1024) { unavailable(); return; }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()
        || !doc.object().value(QStringLiteral("ok")).toBool()) { unavailable(); return; }
    const QJsonObject snapshot = doc.object().value(QStringLiteral("payload")).toObject();
    const QString state = snapshot.value(QStringLiteral("state")).toString();
    static const QStringList states{QStringLiteral("not_registered"), QStringLiteral("registered"),
        QStringLiteral("starting"), QStringLiteral("waiting_for_web_setup"), QStringLiteral("applying"),
        QStringLiteral("connected"), QStringLiteral("error"), QStringLiteral("stopping"), QStringLiteral("paused")};
    if (!states.contains(state) || !snapshot.value(QStringLiteral("running")).isBool()) { unavailable(); return; }
    clearRequest();
    m_state = state;
    m_foregroundStatus = false;
    if (state != QLatin1String("waiting_for_web_setup")) m_expired = false;
    m_running = snapshot.value(QStringLiteral("running")).toBool();
    m_code.clear();
    m_expiresAt = 0;
    const QString code = snapshot.value(QStringLiteral("code")).toString();
    static const QRegularExpression format(QStringLiteral("^[0-9ABCDEFGHJKMNPQRSTVWXYZ]{5}-[0-9ABCDEFGHJKMNPQRSTVWXYZ]{5}$"));
    const qint64 expiry = snapshot.value(QStringLiteral("expires_at")).toInteger();
    if (m_running && state == QLatin1String("waiting_for_web_setup") && expiry > 0
        && expiry <= QDateTime::currentSecsSinceEpoch()) m_expired = true;
    if (m_visible && online() && m_running && state == QLatin1String("waiting_for_web_setup")
        && code.size() == 11 && format.match(code).hasMatch() && expiry > QDateTime::currentSecsSinceEpoch()
        && expiry <= QDateTime::currentSecsSinceEpoch() + 3600) {
        m_code = code;
        m_expired = false;
        m_expiresAt = expiry;
    }
    emit changed();
}
