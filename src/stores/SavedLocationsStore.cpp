#include "SavedLocationsStore.h"
#include "repositories/MdbRepository.h"
#include "services/SavedLocationsService.h"
#include "services/NavigationService.h"
#include "services/ToastService.h"
#include "stores/GpsStore.h"
#include "services/RoadInfoService.h"
#include "models/Enums.h"

#include <QDebug>

SavedLocationsStore::SavedLocationsStore(MdbRepository *repo,
                                           SavedLocationsService *service,
                                           GpsStore *gps, RoadInfoService *roadInfo,
                                           NavigationService *nav,
                                           ToastService *toast, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_gps(gps)
    , m_roadInfo(roadInfo)
    , m_nav(nav)
    , m_toast(toast)
{
    // Reload when settings data arrives from the Redis worker thread.
    // The initial load() in the constructor may run against an empty cache
    // (worker not started yet), so this ensures we pick up the data once it arrives.
    connect(repo, &MdbRepository::fieldsUpdated, this,
            [this](const QString &channel, const FieldMap &) {
        if (channel == QLatin1String("settings"))
            load();
    });

    load();
}

QVariantList SavedLocationsStore::locations() const
{
    QVariantList list;
    for (const auto &loc : m_locations) {
        QVariantMap m;
        m[QStringLiteral("id")] = loc.id;
        m[QStringLiteral("latitude")] = loc.latitude;
        m[QStringLiteral("longitude")] = loc.longitude;
        m[QStringLiteral("label")] = loc.label;
        m[QStringLiteral("createdAt")] = loc.createdAt.toString(Qt::ISODate);
        m[QStringLiteral("lastUsedAt")] = loc.lastUsedAt.toString(Qt::ISODate);
        list.append(m);
    }
    return list;
}

void SavedLocationsStore::load()
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_locations = m_service->loadAll();

    m_isLoading = false;
    emit isLoadingChanged();
    emit locationsChanged();
}

void SavedLocationsStore::saveCurrentLocation()
{
    if (!m_gps->hasValidGps()) {
        m_toast->showError(QStringLiteral("No GPS position available"));
        return;
    }
    double lat = m_gps->latitude();
    double lng = m_gps->longitude();

    if (m_locations.size() >= SavedLocationsService::MaxLocations) {
        m_toast->showError(QStringLiteral("Maximum saved locations reached"));
        return;
    }

    SavedLocation loc;
    loc.latitude = lat;
    loc.longitude = lng;

    // Look up the nearest address from offline vector tiles
    QString address = m_roadInfo->lookupNearestAddress(lat, lng);
    if (!address.isEmpty()) {
        loc.label = address;
    } else {
        loc.label = QString::number(lat, 'f', 5) + QStringLiteral(", ") + QString::number(lng, 'f', 5);
    }

    if (m_service->save(loc)) {
        m_toast->showSuccess(QStringLiteral("Location saved"));
        load();
    } else {
        m_toast->showError(QStringLiteral("Failed to save location"));
    }
}

void SavedLocationsStore::deleteLocation(int id)
{
    if (m_service->remove(id)) {
        load();
        m_toast->showInfo(QStringLiteral("Location deleted"));
    }
}

void SavedLocationsStore::navigateToLocation(int id)
{
    // Snapshot before any external call. updateLastUsed() and
    // setDestination() both publish on the settings/navigation channels,
    // which fires fieldsUpdated synchronously and re-enters load() — that
    // reassigns m_locations, invalidating `loc`. COW-copy the QString first.
    double lat = 0, lng = 0;
    QString label;
    bool found = false;
    for (const auto &loc : m_locations) {
        if (loc.id == id) {
            lat = loc.latitude;
            lng = loc.longitude;
            label = loc.label;
            found = true;
            break;
        }
    }
    if (!found) return;

    m_service->updateLastUsed(id);
    m_nav->setDestination(lat, lng, label);
    load();
}
