#include "SavedLocationsService.h"
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
    QSet<int> usedQuickSlots;
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
        loc.quickSlot = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("quick-slot"))).toInt();
        if (loc.quickSlot < 0 || loc.quickSlot > 2
            || (loc.quickSlot > 0 && usedQuickSlots.contains(loc.quickSlot))) {
            loc.quickSlot = 0;
        } else if (loc.quickSlot > 0) {
            usedQuickSlots.insert(loc.quickSlot);
        }
        loc.quickIcon = m_repo->get(QStringLiteral("settings"), fieldKey(i, QStringLiteral("quick-icon")));
        if (loc.quickIcon != QLatin1String("home") && loc.quickIcon != QLatin1String("work")
            && loc.quickIcon != QLatin1String("favorite")) {
            loc.quickIcon = QStringLiteral("place");
        }
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
    if (!existing) {
        m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("quick-slot")),
                    QStringLiteral("0"), false);
    }
    const QString uuid = existing
        ? ensureUuid(slot)
        : isUuid(location.uuid) ? location.uuid : newUuid();
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("uuid")), uuid, false);
    const QString icon = location.quickIcon == QLatin1String("home")
                      || location.quickIcon == QLatin1String("work")
                      || location.quickIcon == QLatin1String("favorite")
        ? location.quickIcon : QStringLiteral("place");
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("quick-icon")),
                icon, false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("created-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(slot, QStringLiteral("last-used-at")),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate), false);

    if (!setQuickSlot(slot, qBound(0, location.quickSlot, 2)))
        return false;
    // setQuickSlot() publishes when metadata changes; this publication also
    // persists the coordinate/label writes when the requested slot was unchanged.
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
        QStringLiteral("quick-slot"), QStringLiteral("quick-icon"), QStringLiteral("uuid"),
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

bool SavedLocationsService::setQuickSlot(int id, int quickSlot)
{
    if (quickSlot < 0 || quickSlot > 2)
        return false;

    QList<SavedLocation> locations = loadAll();
    auto target = std::find_if(locations.begin(), locations.end(),
                               [id](const SavedLocation &loc) { return loc.id == id; });
    if (target == locations.end())
        return false;
    const int persistedSlot = m_repo->get(QStringLiteral("settings"),
                                          fieldKey(id, QStringLiteral("quick-slot"))).toInt();
    if (target->quickSlot == quickSlot && persistedSlot == quickSlot)
        return true;

    const int oldSlot = target->quickSlot;
    auto displaced = quickSlot == 0 ? locations.end()
                                    : std::find_if(locations.begin(), locations.end(),
                                        [id, quickSlot](const SavedLocation &loc) {
                                            return loc.id != id && loc.quickSlot == quickSlot;
                                        });
    target->quickSlot = quickSlot;
    if (displaced != locations.end())
        displaced->quickSlot = oldSlot >= 1 && oldSlot <= 2 ? oldSlot : 0;

    if (!updateQuickMenu(target->id, target->quickSlot, target->quickIcon))
        return false;
    return displaced == locations.end()
        || updateQuickMenu(displaced->id, displaced->quickSlot, displaced->quickIcon);
}

bool SavedLocationsService::setQuickIcon(int id, const QString &quickIcon)
{
    const QList<SavedLocation> locations = loadAll();
    const auto target = std::find_if(locations.cbegin(), locations.cend(),
                                     [id](const SavedLocation &loc) { return loc.id == id; });
    return target != locations.cend()
        && updateQuickMenu(id, target->quickSlot, quickIcon);
}

bool SavedLocationsService::updateQuickMenu(int id, int quickSlot, const QString &quickIcon)
{
    if (id < 0 || id >= MaxLocations || quickSlot < 0 || quickSlot > 2)
        return false;
    if (m_repo->get(QStringLiteral("settings"), fieldKey(id, QStringLiteral("latitude"))).isEmpty())
        return false;

    const QString icon = quickIcon == QLatin1String("home")
                      || quickIcon == QLatin1String("work")
                      || quickIcon == QLatin1String("favorite")
        ? quickIcon : QStringLiteral("place");
    m_repo->set(QStringLiteral("settings"), fieldKey(id, QStringLiteral("quick-slot")),
                QString::number(quickSlot), false);
    m_repo->set(QStringLiteral("settings"), fieldKey(id, QStringLiteral("quick-icon")),
                icon, false);
    m_repo->publish(QStringLiteral("settings"),
                    QStringLiteral("%1.%2").arg(QLatin1String(AppConfig::savedLocationsPrefix)).arg(id));
    return true;
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
