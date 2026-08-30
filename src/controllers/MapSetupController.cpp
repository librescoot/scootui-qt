#include "MapSetupController.h"

#include "MapSetupPolicy.h"
#include "core/Navigator.h"
#include "services/NavigationAvailabilityService.h"
#include "services/MapDownloadService.h"
#include "stores/InternetStore.h"
#include "stores/GpsStore.h"
#include "models/Enums.h"

namespace {

MapSetupPolicy::Inputs snapshot(const Navigator *navigator,
                                const NavigationAvailabilityService *availability,
                                const InternetStore *internet, const GpsStore *gps,
                                const MapDownloadService *download)
{
    MapSetupPolicy::Inputs in;
    if (navigator)
        in.mode = navigator->setupMode();
    if (availability) {
        in.mapsOk = availability->localDisplayMapsAvailable();
        in.routingOk = availability->routingAvailable();
    }
    if (download) {
        in.updateAvailable = download->updateAvailable();
        in.hasPartialDisplay = download->hasPartialDisplayDownload();
        in.hasPartialRouting = download->hasPartialRoutingDownload();
        in.downloadStatus = download->status();
    }
    if (internet)
        in.online = internet->modemState()
            == static_cast<int>(ScootEnums::ModemState::Connected);
    if (gps)
        in.hasGps = gps->hasValidGps();
    return in;
}

}

MapSetupController::MapSetupController(Navigator *navigator,
                                       NavigationAvailabilityService *availability,
                                       InternetStore *internet, GpsStore *gps,
                                       MapDownloadService *download,
                                       QObject *parent)
    : QObject(parent)
    , m_navigator(navigator)
    , m_availability(availability)
    , m_internet(internet)
    , m_gps(gps)
    , m_download(download)
{
    if (m_navigator)
        connect(m_navigator, &Navigator::setupModeChanged, this, &MapSetupController::policyChanged);
    if (m_availability)
        connect(m_availability, &NavigationAvailabilityService::availabilityChanged,
                this, &MapSetupController::policyChanged);
    if (m_download) {
        connect(m_download, &MapDownloadService::statusChanged, this, &MapSetupController::policyChanged);
        connect(m_download, &MapDownloadService::updateAvailableChanged, this, &MapSetupController::policyChanged);
        connect(m_download, &MapDownloadService::partialStateChanged, this, &MapSetupController::policyChanged);
        connect(m_download, &MapDownloadService::estimatesChanged, this, &MapSetupController::policyChanged);
        // A finished download changes what is installed; ask the
        // availability service to look again.
        connect(m_download, &MapDownloadService::downloadComplete, this, [this]() {
            if (m_availability)
                m_availability->recheck();
        });
    }
    if (m_internet) {
        connect(m_internet, &InternetStore::modemStateChanged, this, &MapSetupController::policyChanged);
        connect(m_internet, &InternetStore::modemStateChanged, this, &MapSetupController::maybeResolveRegion);
    }
    if (m_gps)
        connect(m_gps, &GpsStore::hasValidGpsChanged, this, &MapSetupController::maybeResolveRegion);

    maybeResolveRegion();
}

// Resolve the region name as soon as GPS and connectivity line up, so the
// screen can print region and size before the rider does anything.
void MapSetupController::maybeResolveRegion()
{
    if (!m_download || !m_gps || !m_internet)
        return;
    const bool online = m_internet->modemState()
        == static_cast<int>(ScootEnums::ModemState::Connected);
    if (m_gps->hasValidGps() && online && m_download->regionName().isEmpty())
        m_download->resolveRegion(m_gps->latitude(), m_gps->longitude());
}

bool MapSetupController::showDisplayRow() const
{
    return MapSetupPolicy::showDisplayRow(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

bool MapSetupController::showRoutingRow() const
{
    return MapSetupPolicy::showRoutingRow(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

bool MapSetupController::willDownloadDisplay() const
{
    return MapSetupPolicy::willDownloadDisplay(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

bool MapSetupController::willDownloadRouting() const
{
    return MapSetupPolicy::willDownloadRouting(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

bool MapSetupController::willDownloadAnything() const
{
    return MapSetupPolicy::willDownloadAnything(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

bool MapSetupController::canDownload() const
{
    return MapSetupPolicy::canDownload(snapshot(m_navigator, m_availability, m_internet, m_gps, m_download));
}

int MapSetupController::buttonAction() const
{
    return static_cast<int>(MapSetupPolicy::buttonAction(
        snapshot(m_navigator, m_availability, m_internet, m_gps, m_download)));
}

int MapSetupController::title() const
{
    return static_cast<int>(MapSetupPolicy::title(
        snapshot(m_navigator, m_availability, m_internet, m_gps, m_download)));
}

int MapSetupController::body() const
{
    return static_cast<int>(MapSetupPolicy::body(
        snapshot(m_navigator, m_availability, m_internet, m_gps, m_download)));
}

double MapSetupController::estimatedDownloadBytes() const
{
    if (!m_download)
        return 0;
    const auto in = snapshot(m_navigator, m_availability, m_internet, m_gps, m_download);
    double total = 0;
    if (MapSetupPolicy::willDownloadDisplay(in))
        total += m_download->estimatedDisplayBytes();
    if (MapSetupPolicy::willDownloadRouting(in))
        total += m_download->estimatedRoutingBytes();
    return total;
}

void MapSetupController::triggerDownload()
{
    if (!m_download || !m_gps)
        return;
    const auto in = snapshot(m_navigator, m_availability, m_internet, m_gps, m_download);
    if (!MapSetupPolicy::canDownload(in))
        return;
    m_download->startDownload(m_gps->latitude(), m_gps->longitude(),
                              MapSetupPolicy::willDownloadDisplay(in),
                              MapSetupPolicy::willDownloadRouting(in));
}
