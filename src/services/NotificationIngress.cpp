#include "NotificationIngress.h"
#include "NotificationService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace {
const QString channel = QStringLiteral("scootui:notification");
}

NotificationIngress::NotificationIngress(MdbRepository *repository,
                                         NotificationService *notifications)
    : QObject(notifications), m_repository(repository), m_notifications(notifications)
{
    m_subscription = repository->subscribe(channel, [this](const QString &, const QString &message) {
        // Repository cache refreshes use "*"; this channel only consumes commands.
        if (message != QLatin1String("*"))
            receive(message);
    });
}

NotificationIngress::~NotificationIngress()
{
    if (m_repository)
        m_repository->unsubscribe(channel, m_subscription);
}

bool NotificationIngress::reject(const QString &reason)
{
    emit rejected(reason);
    return false;
}

bool NotificationIngress::receive(const QString &message)
{
    if (message.size() > 4096)
        return reject(QStringLiteral("message exceeds 4096 bytes"));

    const auto bytes = message.toUtf8();
    if (bytes.size() > 4096)
        return reject(QStringLiteral("message exceeds 4096 bytes"));

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return reject(QStringLiteral("expected a JSON object"));

    const auto object = document.object();
    static const QSet<QString> fields = {QStringLiteral("id"),     QStringLiteral("source"),
                                         QStringLiteral("action"), QStringLiteral("title"),
                                         QStringLiteral("body"),   QStringLiteral("severity"),
                                         QStringLiteral("ttl_ms")};
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!fields.contains(it.key()))
            return reject(QStringLiteral("unknown field"));
        if (it.key() != QLatin1String("ttl_ms") && !it.value().isString())
            return reject(QStringLiteral("text fields must be strings"));
    }

    const QString source =
        object.value(QStringLiteral("source")).toString(QStringLiteral("external"));
    const QString id = object.value(QStringLiteral("id")).toString();
    static const QRegularExpression identifier(QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9._-]{0,63}\\z"));
    if (!identifier.match(source).hasMatch() || !identifier.match(id).hasMatch())
        return reject(QStringLiteral("id and source must be 1-64 ASCII letters, digits, dots, "
                                     "underscores or hyphens, starting with a letter or digit"));

    const QString action = object.value(QStringLiteral("action")).toString(QStringLiteral("show"));
    // Keep external IDs separate from producer-owned entries.
    const QString registryId = QStringLiteral("external:%1:%2").arg(source, id);
    if (action == QLatin1String("dismiss")) {
        if (object.contains(QStringLiteral("title")) || object.contains(QStringLiteral("body")) ||
            object.contains(QStringLiteral("severity")) ||
            object.contains(QStringLiteral("ttl_ms")))
            return reject(QStringLiteral("dismiss accepts only action, source and id"));
        m_notifications->clearEvent(registryId);
        return true;
    }
    if (action != QLatin1String("show"))
        return reject(QStringLiteral("action must be show or dismiss"));

    const QString title = object.value(QStringLiteral("title")).toString().trimmed();
    const QString body = object.value(QStringLiteral("body")).toString();
    if (title.isEmpty() || title.size() > 120 || body.size() > 512)
        return reject(QStringLiteral("title must be 1-120 characters; body at most 512"));

    const QString severity =
        object.value(QStringLiteral("severity")).toString(QStringLiteral("info"));
    int priority = 4;
    if (severity == QLatin1String("critical") || severity == QLatin1String("error"))
        priority = 0;
    else if (severity == QLatin1String("warning"))
        priority = 2;
    else if (severity == QLatin1String("success"))
        priority = 3;
    else if (severity == QLatin1String("debug"))
        priority = 5;
    else if (severity != QLatin1String("info"))
        return reject(QStringLiteral("severity must be debug, info, success, warning, error or critical"));

    int ttlMs = 10000;
    if (object.contains(QStringLiteral("ttl_ms"))) {
        const auto ttl = object.value(QStringLiteral("ttl_ms"));
        const double value = ttl.toDouble(-1);
        if (!ttl.isDouble() || value < 1000 || value > 60000 || std::floor(value) != value)
            return reject(QStringLiteral("ttl_ms must be an integer from 1000 to 60000"));
        ttlMs = static_cast<int>(value);
    }

    m_notifications->publishEvent(registryId, QStringLiteral("external:%1").arg(source), title,
                                  body, priority, severity, ttlMs);
    return true;
}
