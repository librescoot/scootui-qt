#include "RecentDestinationsService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>

RecentDestinationsService::RecentDestinationsService(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

QList<RecentDestination> RecentDestinationsService::loadAll() const
{
    QList<RecentDestination> dests;
    for (int i = 0; i < MaxRecents; ++i) {
        QString lat = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("latitude")));
        QString lng = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("longitude")));
        if (lat.isEmpty() || lng.isEmpty())
            continue;

        RecentDestination d;
        d.id = i;
        d.latitude = lat.toDouble();
        d.longitude = lng.toDouble();
        d.label = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("label")));
        QString usedAt = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("used-at")));
        if (!usedAt.isEmpty())
            d.usedAt = QDateTime::fromString(usedAt, Qt::ISODate);

        if (d.latitude != 0 && d.longitude != 0)
            dests.append(d);
    }
    return dests;
}

bool RecentDestinationsService::save(const RecentDestination &dest)
{
    int slot = (dest.id >= 0 && dest.id < MaxRecents) ? dest.id : findFreeSlot(loadAll());
    if (slot < 0) {
        qWarning() << "RecentDestinationsService: No free slot available";
        return false;
    }

    QDateTime ts = dest.usedAt.isValid() ? dest.usedAt : QDateTime::currentDateTimeUtc();

    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("latitude")),
                QString::number(dest.latitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("longitude")),
                QString::number(dest.longitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("label")),
                dest.label, false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("used-at")),
                ts.toString(Qt::ISODate), false);
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::recentDestinationsPrefix)).arg(slot));
    return true;
}

bool RecentDestinationsService::remove(int id)
{
    if (id < 0 || id >= MaxRecents)
        return false;

    QStringList fields = {
        QStringLiteral("latitude"), QStringLiteral("longitude"),
        QStringLiteral("label"), QStringLiteral("used-at")
    };
    for (const auto &f : fields) {
        m_repo->hdel(QStringLiteral("settings"), fieldKey(id, f));
    }
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::recentDestinationsPrefix)).arg(id));
    return true;
}

QString RecentDestinationsService::fieldKey(int id, const QString &field) const
{
    return QStringLiteral("%1.%2.%3")
        .arg(QLatin1String(AppConfig::recentDestinationsPrefix))
        .arg(id)
        .arg(field);
}

int RecentDestinationsService::findFreeSlot(const QList<RecentDestination> &existing) const
{
    QSet<int> used;
    for (const auto &d : existing)
        used.insert(d.id);
    for (int i = 0; i < MaxRecents; ++i) {
        if (!used.contains(i))
            return i;
    }
    return -1;
}
