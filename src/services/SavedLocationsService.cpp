#include "SavedLocationsService.h"
#include "DestinationRpc.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"

#include <QDebug>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

bool isUuid(const QString &value)
{
    return !QUuid(value).isNull();
}

} // namespace

SavedLocationsService::SavedLocationsService(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
    , m_rpc(new DestinationRpc(repo, this))
{
}

QList<SavedLocation> SavedLocationsService::loadAll()
{
    QList<SavedLocation> locations;
    for (int i = 0; i < MaxLocations; ++i) {
        QString lat = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("latitude")));
        QString lng = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("longitude")));
        if (lat.isEmpty() || lng.isEmpty())
            continue;

        SavedLocation loc;
        loc.id = i;
        loc.uuid = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("uuid")));
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
    QVariantMap args;
    if (location.id >= 0 && location.id < MaxLocations)
        args.insert(QStringLiteral("id"), location.id);
    args.insert(QStringLiteral("latitude"), location.latitude);
    args.insert(QStringLiteral("longitude"), location.longitude);
    args.insert(QStringLiteral("label"), location.label);

    QVariantMap reply;
    if (!m_rpc->call(QStringLiteral("destination.save"), args, &reply)) {
        qWarning() << "SavedLocationsService: destination save failed";
        return false;
    }
    return true;
}

bool SavedLocationsService::remove(int id)
{
    if (id < 0 || id >= MaxLocations)
        return false;

    QVariantMap reply;
    if (!m_rpc->call(QStringLiteral("destination.delete"),
                     {{QStringLiteral("id"), id}}, &reply)) {
        qWarning() << "SavedLocationsService: destination delete failed";
        return false;
    }
    return true;
}

bool SavedLocationsService::updateLastUsed(int id)
{
    if (id < 0 || id >= MaxLocations)
        return false;

    QVariantMap reply;
    if (!m_rpc->call(QStringLiteral("destination.touch"),
                     {{QStringLiteral("id"), id}}, &reply)) {
        qWarning() << "SavedLocationsService: destination touch failed";
        return false;
    }
    return true;
}

QList<LegacyQuickAssignment> SavedLocationsService::loadLegacyQuickAssignments() const
{
    QList<LegacyQuickAssignment> assignments;
    QSet<QString> seen;
    for (int i = 0; i < MaxLocations; ++i) {
        if (m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("latitude"))).isEmpty())
            continue;
        bool slotOk = false;
        const int slot = m_repo->get(QStringLiteral("settings"),
                                     fieldKey(i, QStringLiteral("quick-slot"))).toInt(&slotOk);
        if (!slotOk || slot < 1)
            continue;
        LegacyQuickAssignment assignment;
        assignment.id = i;
        assignment.slot = slot;
        assignment.uuid = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("uuid")));
        if (!isUuid(assignment.uuid) || seen.contains(assignment.uuid.toLower()))
            continue;
        seen.insert(assignment.uuid.toLower());
        const QString icon = m_repo->get(QStringLiteral("settings"),
                                         fieldKey(i, QStringLiteral("quick-icon")));
        assignment.icon = (icon == QLatin1String("home") || icon == QLatin1String("work")
                           || icon == QLatin1String("favorite") || icon == QLatin1String("place"))
            ? icon : QStringLiteral("place");
        assignments.append(assignment);
    }
    std::stable_sort(assignments.begin(), assignments.end(),
                     [](const LegacyQuickAssignment &left, const LegacyQuickAssignment &right) {
                         return left.slot < right.slot;
                     });
    return assignments;
}

QString SavedLocationsService::fieldKey(int id, const QString &field) const
{
    return QStringLiteral("%1.%2.%3")
        .arg(QLatin1String(AppConfig::savedLocationsPrefix))
        .arg(id)
        .arg(field);
}
