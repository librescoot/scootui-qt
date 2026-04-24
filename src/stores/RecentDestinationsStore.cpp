#include "RecentDestinationsStore.h"
#include "repositories/MdbRepository.h"
#include "services/RecentDestinationsService.h"
#include "services/SavedLocationsService.h"
#include "services/NavigationService.h"
#include "services/ToastService.h"
#include "models/SavedLocation.h"

#include <QDebug>
#include <QtMath>
#include <algorithm>

RecentDestinationsStore::RecentDestinationsStore(MdbRepository *repo,
                                                   RecentDestinationsService *service,
                                                   SavedLocationsService *savedService,
                                                   NavigationService *nav,
                                                   ToastService *toast, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_savedService(savedService)
    , m_nav(nav)
    , m_toast(toast)
{
    // Mirror SavedLocationsStore: when the settings channel repopulates
    // from the Redis worker thread, reload so we pick up persisted entries.
    connect(repo, &MdbRepository::fieldsUpdated, this,
            [this](const QString &channel, const FieldMap &) {
        if (channel == QLatin1String("settings"))
            load();
    });

    load();
}

QVariantList RecentDestinationsStore::destinations() const
{
    QVariantList list;
    for (const auto &d : m_destinations) {
        QVariantMap m;
        m[QStringLiteral("id")] = d.id;
        m[QStringLiteral("latitude")] = d.latitude;
        m[QStringLiteral("longitude")] = d.longitude;
        m[QStringLiteral("label")] = d.label;
        m[QStringLiteral("usedAt")] = d.usedAt.toString(Qt::ISODate);
        list.append(m);
    }
    return list;
}

void RecentDestinationsStore::load()
{
    m_destinations = m_service->loadAll();
    // Newest first (by usedAt desc).
    std::sort(m_destinations.begin(), m_destinations.end(),
              [](const RecentDestination &a, const RecentDestination &b) {
        return a.usedAt > b.usedAt;
    });
    emit destinationsChanged();
}

void RecentDestinationsStore::push(double lat, double lng, const QString &label)
{
    if (lat == 0 && lng == 0)
        return;

    // Same-place dedupe: bump the timestamp on the existing entry instead
    // of adding a near-duplicate when the rider re-navigates to somewhere
    // they've just been.
    int existingId = findExistingWithinProximity(lat, lng);
    if (existingId >= 0) {
        RecentDestination d;
        d.id = existingId;
        d.latitude = lat;
        d.longitude = lng;
        d.label = label.isEmpty() ? QString() : label;
        // Preserve existing label if the new one is empty.
        if (label.isEmpty()) {
            for (const auto &existing : m_destinations) {
                if (existing.id == existingId) {
                    d.label = existing.label;
                    break;
                }
            }
        }
        d.usedAt = QDateTime::currentDateTimeUtc();
        m_service->save(d);
        load();
        return;
    }

    // No free slot — evict the oldest entry so the new one fits.
    int slot = -1;
    if (m_destinations.size() >= RecentDestinationsService::MaxRecents) {
        slot = evictOldestSlot();
        if (slot < 0) {
            qWarning() << "RecentDestinationsStore: eviction failed, skipping push";
            return;
        }
        m_service->remove(slot);
    }

    RecentDestination d;
    d.id = slot; // -1 lets the service pick a free slot; or the slot we just evicted
    d.latitude = lat;
    d.longitude = lng;
    d.label = label;
    d.usedAt = QDateTime::currentDateTimeUtc();
    m_service->save(d);
    load();
}

void RecentDestinationsStore::navigateToRecent(int id)
{
    // Snapshot first — setDestination re-enters push() (which reassigns
    // m_destinations), invalidating the iterator and the QString
    // reference if we hadn't COW-copied the label.
    double lat = 0, lng = 0;
    QString label;
    bool found = false;
    for (const auto &d : m_destinations) {
        if (d.id == id) {
            lat = d.latitude;
            lng = d.longitude;
            label = d.label;
            found = true;
            break;
        }
    }
    if (found)
        m_nav->setDestination(lat, lng, label);
}

void RecentDestinationsStore::promoteToSaved(int id)
{
    double lat = 0, lng = 0;
    QString label;
    bool found = false;
    for (const auto &d : m_destinations) {
        if (d.id == id) {
            lat = d.latitude;
            lng = d.longitude;
            label = d.label;
            found = true;
            break;
        }
    }
    if (!found) return;

    SavedLocation s;
    s.latitude = lat;
    s.longitude = lng;
    s.label = label;
    if (m_savedService->save(s)) {
        m_service->remove(id);
        m_toast->showSuccess(QStringLiteral("Saved to favorites"));
        load();
    } else {
        m_toast->showError(QStringLiteral("Could not save"));
    }
}

void RecentDestinationsStore::deleteRecent(int id)
{
    if (m_service->remove(id))
        load();
}

int RecentDestinationsStore::findExistingWithinProximity(double lat, double lng, double meters) const
{
    const double latRad = qDegreesToRadians(lat);
    const double mPerDegLat = 111320.0;
    const double mPerDegLng = 111320.0 * qCos(latRad);
    for (const auto &d : m_destinations) {
        double dLatM = (lat - d.latitude) * mPerDegLat;
        double dLngM = (lng - d.longitude) * mPerDegLng;
        double distM = std::sqrt(dLatM * dLatM + dLngM * dLngM);
        if (distM <= meters)
            return d.id;
    }
    return -1;
}

int RecentDestinationsStore::evictOldestSlot() const
{
    if (m_destinations.isEmpty())
        return -1;
    // m_destinations is already newest-first, so tail is oldest.
    return m_destinations.last().id;
}
