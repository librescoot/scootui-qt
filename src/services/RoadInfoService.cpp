#include "RoadInfoService.h"
#include "MapService.h"
#include "NavigationService.h"
#include "stores/GpsStore.h"
#include "stores/SpeedLimitStore.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

RoadInfoService::RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                                   NavigationService *navigation,
                                   QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_speedLimit(speedLimit)
    , m_navigation(navigation)
{
    m_lastUpdate.start();

    m_worker = new RoadInfoWorker;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("roadinfo"));
    m_thread.start();
    // Open the mbtiles off the UI thread as soon as the loop spins up.
    QMetaObject::invokeMethod(m_worker, &RoadInfoWorker::reload,
                              Qt::QueuedConnection);

    // Connect the GPS signals unconditionally, even if the mbtiles isn't open
    // yet. On a cold boot scootui can start before /data is mounted, so the
    // file is absent here — but the route-driven road-name/speed path in
    // updateRoadInfo() needs no tile DB, and the worker self-heals the DB
    // open once the file appears. Gating the connects on the DB used to leave
    // the whole pill dead (no writer) for the entire session in that race.
    connect(gps, &GpsStore::sampleChanged, this, &RoadInfoService::onGpsChanged);
}

RoadInfoService::~RoadInfoService()
{
    if (m_thread.isRunning()) {
        // The QSqlDatabase connection is thread-affine: close it on the
        // worker thread while its loop still runs, then stop the thread.
        QMetaObject::invokeMethod(m_worker, &RoadInfoWorker::closeDb,
                                  Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
    delete m_worker;
}

void RoadInfoService::setMapService(MapService *map)
{
    if (m_map == map)
        return;
    if (m_map)
        disconnect(m_map, nullptr, this, nullptr);
    m_map = map;
    m_updateCadence.reset();
    if (m_map) {
        connect(m_map, &MapService::vehiclePositionChanged,
                this, &RoadInfoService::onVehiclePositionChanged);
    }
}

void RoadInfoService::reloadMbtiles()
{
    QMetaObject::invokeMethod(m_worker, &RoadInfoWorker::reload,
                              Qt::QueuedConnection);
}

void RoadInfoService::clearRoadInfo()
{
    m_speedLimit->setRoadNameDirect(QString());
    m_speedLimit->setRoadRefsDirect(QString());
    m_speedLimit->setRoadTypeDirect(QString());
    m_speedLimit->setSpeedLimitDirect(QString());
    m_speedLimit->setRoadBearingDirect(-1);
}

void RoadInfoService::countMissAndMaybeClear()
{
    clearRoadMatch();
    if (++m_consecutiveMisses >= ClearAfterMisses)
        clearRoadInfo();
}

void RoadInfoService::clearRoadMatch()
{
    if (!m_hasConfidentRoadMatch && m_previousMatchKey.isEmpty())
        return;
    m_hasConfidentRoadMatch = false;
    m_previousMatchKey.clear();
    m_matchDistanceMeters = 0;
    emit roadMatchChanged();
}

void RoadInfoService::onGpsChanged()
{
    if (m_map && m_map->hasVehiclePosition())
        return;
    if (!m_gps || !m_gps->hasValidGps()) {
        m_consecutiveMisses = ClearAfterMisses;
        clearRoadMatch();
        clearRoadInfo();
        return;
    }

    if (m_lastUpdate.elapsed() < FallbackUpdateIntervalMs)
        return;

    m_lastUpdate.restart();
    updateRoadInfo(m_gps->latitude(), m_gps->longitude());
}

void RoadInfoService::onVehiclePositionChanged()
{
    if (!m_map || !m_map->hasVehiclePosition())
        return;
    if (!m_updateCadence.advance())
        return;
    updateRoadInfo(m_map->vehicleLatitude(), m_map->vehicleLongitude());
}

void RoadInfoService::publishRouteAttrs()
{
    m_speedLimit->setRoadNameDirect(m_navigation->currentEdgeName());
    m_speedLimit->setRoadRefsDirect(
        m_navigation->currentEdgeRefs().join(QStringLiteral(", ")));
    m_speedLimit->setRoadTypeDirect(m_navigation->currentEdgeRoadClass());
    const int kph = m_navigation->currentEdgeSpeedLimitKph();
    m_speedLimit->setSpeedLimitDirect(
        kph > 0 ? QString::number(kph) : QString());
}

void RoadInfoService::updateRoadInfo(double lat, double lon)
{
    const bool hasRouteAttrs = m_navigation
        && m_navigation->hasCurrentEdgeAttrs();

    // A complete route edge is authoritative and avoids doing any tile work.
    // Partial trace attributes fall through so missing name/class/speed fields
    // can be filled from the local vector tile.
    if (hasRouteAttrs
        && !m_navigation->currentEdgeName().isEmpty()
        && !m_navigation->currentEdgeRoadClass().isEmpty()
        && m_navigation->currentEdgeSpeedLimitKph() > 0) {
        publishRouteAttrs();
        clearRoadMatch();
        m_consecutiveMisses = 0;
        return;
    }

    if (m_requestInFlight) {
        m_pendingLat = lat;
        m_pendingLon = lon;
        m_hasPendingRequest = true;
        return;
    }
    sendMatchRequest(lat, lon);
}

void RoadInfoService::sendMatchRequest(double lat, double lon)
{
    m_requestInFlight = true;

    RoadMatchRequest request;
    request.lat = lat;
    request.lon = lon;

    const GpsSample gps = m_gps ? m_gps->currentSample() : GpsSample{};
    request.headingReliable = m_gps && gps.hasValidCoordinate()
        && gps.hasFix() && m_gps->timestampAgeMs() <= 2500
        && gps.speedKmh >= 3.0 && std::isfinite(gps.course);
    request.courseDegrees = gps.course;

    double maxDistance = 35.0;
    if (m_map)
        maxDistance = std::clamp(m_map->positionUncertaintyMeters() + 8.0,
                                 15.0, 40.0);
    else if (gps.ephMeters > 0.0)
        maxDistance = std::clamp(gps.ephMeters * 1.5 + 8.0, 15.0, 40.0);
    request.maxDistanceMeters = maxDistance;

    request.previousMatchKey = m_previousMatchKey;
    request.routeName = m_navigation
        ? m_navigation->currentSegmentStreetName() : QString();

    auto *worker = m_worker;
    QMetaObject::invokeMethod(worker, [this, worker, request]() {
        const RoadMatchResult result = worker->matchRoad(request);
        QMetaObject::invokeMethod(this, [this, result]() {
            applyMatchResult(result);
        });
    });
}

void RoadInfoService::applyMatchResult(const RoadMatchResult &result)
{
    m_requestInFlight = false;
    processMatchResult(result);
    if (m_hasPendingRequest) {
        m_hasPendingRequest = false;
        sendMatchRequest(m_pendingLat, m_pendingLon);
    }
}

void RoadInfoService::processMatchResult(const RoadMatchResult &result)
{
    const bool hasRouteAttrs = m_navigation
        && m_navigation->hasCurrentEdgeAttrs();

    if (!result.dbAvailable) {
        if (hasRouteAttrs) {
            publishRouteAttrs();
            clearRoadMatch();
        }
        return;
    }

    if (!result.matched) {
        if (hasRouteAttrs) {
            publishRouteAttrs();
            clearRoadMatch();
            m_consecutiveMisses = 0;
        } else {
            countMissAndMaybeClear();
        }
        return;
    }

    m_consecutiveMisses = 0;
    QString name = result.name;
    QString refs = result.refs;
    QString kind = result.kind;
    QString maxspeed = result.maxspeed;
    if (hasRouteAttrs) {
        if (!m_navigation->currentEdgeName().isEmpty())
            name = m_navigation->currentEdgeName();
        if (!m_navigation->currentEdgeRefs().isEmpty())
            refs = m_navigation->currentEdgeRefs().join(QStringLiteral(", "));
        if (!m_navigation->currentEdgeRoadClass().isEmpty())
            kind = m_navigation->currentEdgeRoadClass();
        if (m_navigation->currentEdgeSpeedLimitKph() > 0)
            maxspeed = QString::number(m_navigation->currentEdgeSpeedLimitKph());
    }
    m_speedLimit->setSpeedLimitDirect(maxspeed);
    m_speedLimit->setRoadNameDirect(name);
    m_speedLimit->setRoadRefsDirect(refs);
    m_speedLimit->setRoadTypeDirect(kind);
    m_speedLimit->setRoadBearingDirect(result.bearingDegrees);

    const bool freeDrive = !m_navigation || !m_navigation->hasRoute();
    const bool confident = freeDrive && result.confident;
    const bool changed = confident != m_hasConfidentRoadMatch
        || result.lat1 != m_matchLat1 || result.lon1 != m_matchLon1
        || result.lat2 != m_matchLat2 || result.lon2 != m_matchLon2;
    m_hasConfidentRoadMatch = confident;
    m_matchLat1 = result.lat1; m_matchLon1 = result.lon1;
    m_matchLat2 = result.lat2; m_matchLon2 = result.lon2;
    m_matchDistanceMeters = result.distanceMeters;
    m_previousMatchKey = confident ? result.key : QString();
    if (changed)
        emit roadMatchChanged();
}

QString RoadInfoService::lookupNearestAddress(double lat, double lon)
{
    QString result;
    if (!m_thread.isRunning())
        return result;
    auto *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, lat, lon]() {
        return worker->lookupNearestAddress(lat, lon);
    }, Qt::BlockingQueuedConnection, &result);
    return result;
}

QVariantList RoadInfoService::streetsInBbox(double minLat, double minLon,
                                              double maxLat, double maxLon)
{
    QVariantList result;
    if (!m_thread.isRunning())
        return result;
    auto *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, minLat, minLon, maxLat, maxLon]() {
        return worker->streetsInBbox(minLat, minLon, maxLat, maxLon);
    }, Qt::BlockingQueuedConnection, &result);
    return result;
}
