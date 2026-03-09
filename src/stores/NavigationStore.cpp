#include "NavigationStore.h"

NavigationStore::NavigationStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings NavigationStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("navigation"), 5000,
        {
            {QStringLiteral("latitude"), QStringLiteral("latitude")},
            {QStringLiteral("longitude"), QStringLiteral("longitude")},
            {QStringLiteral("address"), QStringLiteral("address")},
            {QStringLiteral("timestamp"), QStringLiteral("timestamp")},
            {QStringLiteral("destination"), QStringLiteral("destination")},
        },
        {}, {}
    };
}

void NavigationStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("latitude")) {
        if (value != m_latitude) { m_latitude = value; emit latitudeChanged(); }
    } else if (variable == QLatin1String("longitude")) {
        if (value != m_longitude) { m_longitude = value; emit longitudeChanged(); }
    } else if (variable == QLatin1String("address")) {
        if (value != m_address) { m_address = value; emit addressChanged(); }
    } else if (variable == QLatin1String("timestamp")) {
        if (value != m_timestamp) { m_timestamp = value; emit timestampChanged(); }
    } else if (variable == QLatin1String("destination")) {
        if (value != m_destination) { m_destination = value; emit destinationChanged(); }
    }
}

void NavigationStore::setDestination(const QString &dest)
{
    m_repo->set(QStringLiteral("navigation"), QStringLiteral("destination"), dest);
}

void NavigationStore::clearDestination()
{
    m_repo->hdel(QStringLiteral("navigation"), QStringLiteral("destination"));
}
