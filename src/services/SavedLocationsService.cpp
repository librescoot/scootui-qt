#include "SavedLocationsService.h"
#include "repositories/MdbRepository.h"
#include "core/AppConfig.h"
#include "core/ShortcutMenuItems.h"

#include <QDebug>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

bool isUuid(const QString &value)
{
    return !QUuid(value).isNull();
}

QString newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

SavedLocationsService::SavedLocationsService(MdbRepository *repo, QObject *parent)
    : QObject(parent)
    , m_repo(repo)
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
        loc.uuid = ensureUuid(i);
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

    const bool existing = !m_repo->get(QStringLiteral("settings"),
                                       fieldKey(slot, QStringLiteral("latitude"))).isEmpty();
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("latitude")),
                QString::number(location.latitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("longitude")),
                QString::number(location.longitude, 'f', 7), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("label")),
                location.label, false);
    const QString uuid = existing
        ? ensureUuid(slot)
        : isUuid(location.uuid) ? location.uuid : newUuid();
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("uuid")), uuid, false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("created-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("last-used-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);

    // The record-prefix publication persists the coordinate and label writes.
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(slot));
    return true;
}

bool SavedLocationsService::remove(int id)
{
    if (id < 0 || id >= MaxLocations)
        return false;

    const QString uuid = m_repo->get(QStringLiteral("settings"), fieldKey(id, QStringLiteral("uuid")));
    QStringList fields = {
        QStringLiteral("latitude"), QStringLiteral("longitude"), QStringLiteral("label"),
        QStringLiteral("quick-slot"), QStringLiteral("quick-icon"), QStringLiteral("uuid"),
        QStringLiteral("created-at"), QStringLiteral("last-used-at")
    };
    for (const auto &f : fields) {
        m_repo->hdel(QStringLiteral("settings"), fieldKey(id, f));
    }
    // Notify settings-service so the deletion is persisted to TOML
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(id));
    if (!uuid.isEmpty())
        pruneDestinationItem(uuid);
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

void SavedLocationsService::pruneDestinationItem(const QString &uuid)
{
    bool ok = false;
    const QStringList items = ShortcutMenuItems::parse(
        m_repo->get(QStringLiteral("settings"), QLatin1String(ShortcutMenuItems::SettingsKey)), &ok);
    if (!ok)
        return;
    const QStringList pruned = ShortcutMenuItems::withoutDestination(items, uuid);
    if (pruned.size() == items.size())
        return;
    m_repo->set(QStringLiteral("settings"), QLatin1String(ShortcutMenuItems::SettingsKey),
                ShortcutMenuItems::serialize(pruned));
}

QString SavedLocationsService::ensureUuid(int id)
{
    const QString current = m_repo->get(QStringLiteral("settings"),
                                        fieldKey(id, QStringLiteral("uuid")));
    if (isUuid(current))
        return current;

    const QString uuid = newUuid();
    m_repo->set(QStringLiteral("settings"), fieldKey(id, QStringLiteral("uuid")), uuid, false);
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(id));
    return uuid;
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
