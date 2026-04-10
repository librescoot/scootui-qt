#include "SavedLocationsService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>

SavedLocationsService::SavedLocationsService(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
{
}

QList<SavedLocation> SavedLocationsService::loadAll() const
{
    QList<SavedLocation> locations;
    for (int i = 0; i < MaxLocations; ++i) {
        QString lat = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("latitude")));
        QString lng = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("longitude")));
        if (lat.isEmpty() || lng.isEmpty())
            continue;

        SavedLocation loc;
        loc.id = i;
        loc.latitude = lat.toDouble();
        loc.longitude = lng.toDouble();
        loc.label = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("label")));
        QString createdAt = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("created-at")));
        if (!createdAt.isEmpty())
            loc.createdAt = QDateTime::fromString(createdAt, Qt::ISODate);
        QString lastUsed = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("last-used-at")));
        if (!lastUsed.isEmpty())
            loc.lastUsedAt = QDateTime::fromString(lastUsed, Qt::ISODate);

        if (loc.latitude != 0 && loc.longitude != 0)
            locations.append(loc);
    }
    return locations;
}

bool SavedLocationsService::save(const SavedLocation &location)
{
    int slot = (location.id >= 0 && location.id < MaxLocations) ? location.id : findFreeSlot();
    if (slot < 0) {
        qWarning() << "SavedLocationsService: No free slot available";
        return false;
    }

    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("latitude")),
                QString::number(location.latitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("longitude")),
                QString::number(location.longitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("label")),
                location.label, false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("created-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("last-used-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);
    // Notify settings-service so the new location is persisted to TOML
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(slot));
    return true;
}

bool SavedLocationsService::remove(int id)
{
    if (id < 0 || id >= MaxLocations)
        return false;

    QStringList fields = {
        QStringLiteral("latitude"), QStringLiteral("longitude"), QStringLiteral("label"),
        QStringLiteral("created-at"), QStringLiteral("last-used-at")
    };
    for (const auto &f : fields) {
        m_repo->hdel(QStringLiteral("settings"), fieldKey(id, f));
    }
    // Notify settings-service so the deletion is persisted to TOML
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(id));
    return true;
}

bool SavedLocationsService::updateLastUsed(int id)
{
    if (id < 0 || id >= MaxLocations)
        return false;

    m_repo->set(QStringLiteral("settings"), fieldKey(id, QStringLiteral("last-used-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), true);
    return true;
}

QString SavedLocationsService::fieldKey(int id, const QString &field) const
{
    return QStringLiteral("%1.%2.%3")
        .arg(QLatin1String(AppConfig::savedLocationsPrefix))
        .arg(id)
        .arg(field);
}

int SavedLocationsService::findFreeSlot() const
{
    for (int i = 0; i < MaxLocations; ++i) {
        QString lat = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("latitude")));
        if (lat.isEmpty())
            return i;
    }
    return -1;
}
