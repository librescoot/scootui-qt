#include "SavedLocationsStore.h"
#include "repositories/MdbRepository.h"
#include "services/SavedLocationsService.h"
#include "services/ReverseGeocodingService.h"
#include "services/NavigationService.h"
#include "services/ToastService.h"
#include "stores/GpsStore.h"
#include "models/Enums.h"

#include <QDebug>

SavedLocationsStore::SavedLocationsStore(MdbRepository *repo,
                                           SavedLocationsService *service,
                                           ReverseGeocodingService *geocoding,
                                           GpsStore *gps, NavigationService *nav,
                                           ToastService *toast, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_geocoding(geocoding)
    , m_gps(gps)
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
    // Validate GPS
    if (m_gps->gpsState() != static_cast<int>(ScootEnums::GpsState::FixEstablished)) {
        m_toast->showError(QStringLiteral("No GPS fix available"));
        return;
    }

    double lat = m_gps->latitude();
    double lng = m_gps->longitude();
    if (lat == 0 && lng == 0) {
        m_toast->showError(QStringLiteral("Invalid GPS position"));
        return;
    }

    if (m_locations.size() >= SavedLocationsService::MaxLocations) {
        m_toast->showError(QStringLiteral("Maximum saved locations reached"));
        return;
    }

    SavedLocation loc;
    loc.latitude = lat;
    loc.longitude = lng;
    loc.label = QString::number(lat, 'f', 5) + QStringLiteral(", ") + QString::number(lng, 'f', 5);

    // Try reverse geocoding in background
    connect(m_geocoding, &ReverseGeocodingService::addressResolved, this,
            [this, lat, lng](const QString &address) {
        // Update the label of the most recently saved location matching these coords
        for (auto &loc : m_locations) {
            if (qFuzzyCompare(loc.latitude, lat) && qFuzzyCompare(loc.longitude, lng)) {
                loc.label = address;
                SavedLocation updated = loc;
                updated.label = address;
                m_service->save(updated);
                emit locationsChanged();
                break;
            }
        }
    }, Qt::SingleShotConnection);

    if (m_service->save(loc)) {
        m_toast->showSuccess(QStringLiteral("Location saved"));
        m_geocoding->resolve(lat, lng);
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
    for (const auto &loc : m_locations) {
        if (loc.id == id) {
            m_service->updateLastUsed(id);
            m_nav->setDestination(loc.latitude, loc.longitude, loc.label);
            load();
            return;
        }
    }
}
