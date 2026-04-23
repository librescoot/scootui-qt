#include "FaultEventStore.h"

namespace {
constexpr const char *kStreamKey = "events:faults";

qint64 parseStreamIdMs(const QString &id)
{
    const int dash = id.indexOf(QLatin1Char('-'));
    const QString msPart = dash >= 0 ? id.left(dash) : id;
    bool ok = false;
    qint64 ms = msPart.toLongLong(&ok);
    return ok ? ms : 0;
}
}

FaultEventStore::FaultEventStore(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(m_pollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &FaultEventStore::refresh);
    connect(m_repo, &MdbRepository::streamFetched,
            this, &FaultEventStore::onStreamFetched);
}

void FaultEventStore::start()
{
    refresh();
    m_timer->start();
}

void FaultEventStore::stop()
{
    m_timer->stop();
}

void FaultEventStore::refresh()
{
    m_repo->xrevrange(QString::fromLatin1(kStreamKey), kFetchCount);
}

void FaultEventStore::setPollIntervalMs(int intervalMs)
{
    if (intervalMs <= 0 || intervalMs == m_pollIntervalMs)
        return;
    m_pollIntervalMs = intervalMs;
    m_timer->setInterval(intervalMs);
}

void FaultEventStore::onStreamFetched(const QString &key, const QVariantList &entries)
{
    if (key != QLatin1String(kStreamKey))
        return;

    QVector<FaultEvent> parsed;
    parsed.reserve(entries.size());
    for (const QVariant &v : entries) {
        const QVariantMap entry = v.toMap();
        const QString id = entry.value(QStringLiteral("id")).toString();
        const QVariantMap fields = entry.value(QStringLiteral("fields")).toMap();

        FaultEvent e;
        e.timestampMs = parseStreamIdMs(id);
        e.source = fields.value(QStringLiteral("group")).toString();
        const QString codeStr = fields.value(QStringLiteral("code")).toString();
        bool ok = false;
        e.code = codeStr.toInt(&ok);
        if (!ok)
            continue;
        e.description = fields.value(QStringLiteral("description")).toString();
        parsed.append(e);
    }

    m_events = std::move(parsed);
    emit eventsChanged();
}
