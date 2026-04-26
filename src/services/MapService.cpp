#include "MapService.h"
#include "NavigationService.h"
#include "stores/GpsStore.h"
#include "stores/EngineStore.h"
#include "stores/SettingsStore.h"
#include "stores/ThemeStore.h"
#include "stores/SpeedLimitStore.h"
#include "routing/RouteModels.h"
#include "models/Enums.h"

#include <QDebug>
#include <QVariantMap>
#include <QtMath>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>

// ---------------------------------------------------------------------------
// Earth geometry helpers
// ---------------------------------------------------------------------------

static constexpr double EarthRadius = 6371000.0;
static constexpr double DegToRad = M_PI / 180.0;
static constexpr double RadToDeg = 180.0 / M_PI;

static double haversineDistance(double lat1, double lon1, double lat2, double lon2)
{
    double dLat = (lat2 - lat1) * DegToRad;
    double dLon = (lon2 - lon1) * DegToRad;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1 * DegToRad) * std::cos(lat2 * DegToRad) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    // Clamp to [0, 1] to prevent NaN from sqrt of negative due to precision
    return EarthRadius * 2.0 * std::atan2(std::sqrt(std::max(0.0, a)), std::sqrt(std::max(0.0, 1.0 - a)));
}

static double bearingBetween(double lat1, double lon1, double lat2, double lon2)
{
    double dLon = (lon2 - lon1) * DegToRad;
    double y = std::sin(dLon) * std::cos(lat2 * DegToRad);
    double x = std::cos(lat1 * DegToRad) * std::sin(lat2 * DegToRad) -
               std::sin(lat1 * DegToRad) * std::cos(lat2 * DegToRad) * std::cos(dLon);
    
    // Avoid atan2(0, 0)
    if (std::abs(x) < 1e-10 && std::abs(y) < 1e-10) return 0.0;
    
    double brng = std::atan2(y, x) * RadToDeg;
    return std::fmod(brng + 360.0, 360.0);
}

// Project a point forward by `distance` meters along `bearing` degrees
static void projectForward(double lat, double lng, double bearing, double distance,
                           double &outLat, double &outLng)
{
    double angDist = distance / EarthRadius;
    double bearRad = bearing * DegToRad;
    double latRad = lat * DegToRad;
    double lngRad = lng * DegToRad;

    double sinLat = std::sin(latRad);
    double cosLat = std::cos(latRad);
    double sinD = std::sin(angDist);
    double cosD = std::cos(angDist);

    // Clamp input to asin to [-1, 1] to prevent NaNs
    double val = sinLat * cosD + cosLat * sinD * std::cos(bearRad);
    outLat = std::asin(std::clamp(val, -1.0, 1.0)) * RadToDeg;
    outLng = (lngRad + std::atan2(std::sin(bearRad) * sinD * cosLat,
                                   cosD - sinLat * std::sin(outLat * DegToRad))) * RadToDeg;
}

// Perpendicular distance from (lat, lon) to the great-circle segment (A→B).
// Uses a local equirectangular approximation (valid for segments < ~50 km).
static double distanceToSegment(double lat, double lon,
                                double aLat, double aLon,
                                double bLat, double bLon)
{
    double cosLat = std::cos(lat * DegToRad);
    // Local metric offsets of segment endpoints from the query point
    double ax = (aLon - lon) * EarthRadius * cosLat * DegToRad;
    double ay = (aLat - lat) * EarthRadius * DegToRad;
    double bx = (bLon - lon) * EarthRadius * cosLat * DegToRad;
    double by = (bLat - lat) * EarthRadius * DegToRad;
    double dx = bx - ax;
    double dy = by - ay;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-6)
        return std::hypot(ax, ay);
    // Project origin onto line AB; t ∈ [0,1] clamps to segment
    double t = std::clamp((-ax * dx - ay * dy) / lenSq, 0.0, 1.0);
    double px = ax + t * dx;
    double py = ay + t * dy;
    return std::hypot(px, py);
}

// Closest point on segment A→B to (lat, lng) in the local equirectangular
// frame. Writes the snapped coordinate and the perpendicular distance in m.
static void projectOntoSegment(double lat, double lng,
                                double aLat, double aLon,
                                double bLat, double bLon,
                                double &outLat, double &outLng, double &outDist)
{
    double cosLat = std::cos(lat * DegToRad);
    double ax = (aLon - lng) * EarthRadius * cosLat * DegToRad;
    double ay = (aLat - lat) * EarthRadius * DegToRad;
    double bx = (bLon - lng) * EarthRadius * cosLat * DegToRad;
    double by = (bLat - lat) * EarthRadius * DegToRad;
    double dx = bx - ax;
    double dy = by - ay;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-6) {
        outLat = aLat;
        outLng = aLon;
        outDist = std::hypot(ax, ay);
        return;
    }
    double t = std::clamp((-ax * dx - ay * dy) / lenSq, 0.0, 1.0);
    double px = ax + t * dx;
    double py = ay + t * dy;
    outDist = std::hypot(px, py);
    outLat = lat + (py / EarthRadius) * RadToDeg;
    outLng = lng + (px / (EarthRadius * cosLat)) * RadToDeg;
}

// Signed angular difference in degrees, wrapped to [-180, 180].
static double signedAngleDiff(double a, double b)
{
    double d = std::fmod(a - b, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d < -180.0) d += 360.0;
    return d;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MapService::MapService(GpsStore *gps, EngineStore *engine,
                       NavigationService *navigation, SettingsStore *settings,
                       ThemeStore *theme, SpeedLimitStore *speedLimit,
                       QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_engine(engine)
    , m_navigation(navigation)
    , m_settings(settings)
    , m_theme(theme)
    , m_speedLimit(speedLimit)
    , m_tickTimer(new QTimer(this))
{
    // reloadMbtiles() opens a SQLite connection for the mbtiles validation
    // probe and parses two JSON style files to rewrite tile:// URLs. That's
    // a few hundred ms on iMX6 for work nothing else waits on during
    // createStores(). Queue it on the first event-loop tick so we don't
    // hold up publishReady().
    QTimer::singleShot(0, this, &MapService::reloadMbtiles);

    // --- GPS position updates ---
    connect(m_gps, &GpsStore::latitudeChanged, this, &MapService::onGpsPositionChanged);
    connect(m_gps, &GpsStore::longitudeChanged, this, &MapService::onGpsPositionChanged);

    // --- Route changes ---
    connect(m_navigation, &NavigationService::routeChanged, this, &MapService::onRouteChanged);

    // --- Theme changes ---
    connect(m_theme, &ThemeStore::themeChanged, this, &MapService::onThemeChanged);

    // --- Map type changes (online / offline) ---
    connect(m_settings, &SettingsStore::mapTypeChanged, this, &MapService::onMapTypeChanged);

    // --- Traffic overlay toggle ---
    connect(m_settings, &SettingsStore::mapTrafficOverlayChanged, this, &MapService::onTrafficOverlayChanged);

    // --- Route overview timer (single-shot) ---
    m_overviewTimer = new QTimer(this);
    m_overviewTimer->setSingleShot(true);
    m_overviewTimer->setInterval(OverviewHoldMs);
    connect(m_overviewTimer, &QTimer::timeout, this, &MapService::onOverviewTimeout);

    // --- Dead reckoning timer at 15 Hz ---
    m_tickTimer->setTimerType(Qt::PreciseTimer);
    m_tickTimer->setInterval(static_cast<int>(TickIntervalMs));
    connect(m_tickTimer, &QTimer::timeout, this, &MapService::onDeadReckoningTick);

    m_elapsed.start();
    m_tickTimer->start();
}

MapService::~MapService()
{
    m_tickTimer->stop();
}

void MapService::reloadMbtiles()
{
    QString newPath;

    if (QFile::exists(QStringLiteral("map.mbtiles"))) {
        newPath = QDir::currentPath() + QStringLiteral("/map.mbtiles");
    } else if (QFile::exists(QStringLiteral("/data/maps/map.mbtiles"))) {
        newPath = QStringLiteral("/data/maps/map.mbtiles");
    }

    if (!newPath.isEmpty()) {
        const QString connName = QStringLiteral("mapservice_validate");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(newPath);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
            bool valid = false;
            if (db.open()) {
                QSqlQuery q(db);
                valid = q.exec(QStringLiteral("SELECT count(*) FROM metadata")) && q.next();
                db.close();
            }
            if (!valid) {
                qWarning() << "MapService: mbtiles database is unreadable, falling back to online tiles";
                newPath.clear();
            }
        }
        QSqlDatabase::removeDatabase(connName);
    }

    if (newPath == m_mbtilesPath)
        return;

    if (newPath.isEmpty()) {
        qDebug() << "MapService: no mbtiles found, using online tiles";
    } else {
        qDebug() << "MapService: mbtiles loaded:" << newPath;
    }

    m_mbtilesPath = newPath;
    rebuildStyleUrl();
    loadMbtilesBounds();
}

// ---------------------------------------------------------------------------
// Route waypoints (called from Application.cpp wiring)
// ---------------------------------------------------------------------------

void MapService::setRouteWaypoints(const QVariantList &waypoints)
{
    qDebug() << "MapService: incoming waypoints:" << waypoints.size();
    m_routeShape.clear();
    m_routeShape.reserve(waypoints.size());

    QVariantList coords;
    coords.reserve(waypoints.size());

    for (const QVariant &v : waypoints) {
        const QVariantMap m = v.toMap();
        double lat = m.value(QStringLiteral("latitude")).toDouble();
        double lng = m.value(QStringLiteral("longitude")).toDouble();
        
        // Filter out duplicate points to prevent division by zero/NaN in bearing logic
        if (!m_routeShape.isEmpty()) {
            const auto &last = m_routeShape.last();
            if (std::abs(lat - last.first) < 1e-9 && std::abs(lng - last.second) < 1e-9)
                continue;
        }

        m_routeShape.append({lat, lng});
        
        QVariantMap wp;
        wp[QStringLiteral("latitude")] = lat;
        wp[QStringLiteral("longitude")] = lng;
        coords.append(wp);
    }

    m_routeCoordinates = coords;
    qDebug() << "MapService: stored waypoints:" << m_routeCoordinates.size();
    emit routeCoordinatesChanged();

    // Reset segment tracking. If we already have a DR position, seed the
    // initial match using trajectory-aware matching — Valhalla's reroute
    // starts its shape at the current position, so segment 0 is usually
    // right, but the trajectory check guards against unusual cases where
    // the new shape doesn't begin exactly at the rider's current location.
    if (m_hasInitialPosition && m_routeShape.size() >= 2) {
        double speedKmh = m_engine->speed();
        bool haveTrajectory = speedKmh >= MinSpeedForTrajectoryKmh;
        SegmentMatch seeded = matchRouteSegment(m_drLatitude, m_drLongitude,
                                                 m_displayBearing, haveTrajectory,
                                                 -1);
        m_currentRouteSegment = (seeded.index >= 0 &&
                                  seeded.perpDist < MatchAcceptanceDistance)
                                ? seeded.index : 0;
    } else {
        m_currentRouteSegment = 0;
    }
    m_maxReachedSegment = m_currentRouteSegment;
    m_lastRouteBearing = -1;
    // New shape: reset the snap-lock state machine. Reroute may land far from
    // the rider, in which case break-away will trip again on its own; resetting
    // avoids inheriting a stale "unlocked" state from the previous shape.
    m_drLocked = true;
    m_lockTransitionTimer.invalidate();

    updateRouteGeoJson();
    refreshRouteProjection();
}

void MapService::updateRouteFromNavigation()
{
    auto waypoints = m_navigation->routeWaypoints();
    qDebug() << "MapService: updating route from NavigationService - points:" << waypoints.size();
    QVariantList varList;
    varList.reserve(waypoints.size());
    for (const auto &wp : waypoints) {
        if (!std::isfinite(wp.latitude) || !std::isfinite(wp.longitude))
            continue;
        QVariantMap m;
        m[QStringLiteral("latitude")] = wp.latitude;
        m[QStringLiteral("longitude")] = wp.longitude;
        varList.append(m);
    }
    setRouteWaypoints(varList);
}

void MapService::clearRoute()
{
    m_routeShape.clear();
    m_currentRouteSegment = -1;
    m_maxReachedSegment = -1;
    m_lastRouteBearing = -1;
    m_drLocked = true;
    m_lockTransitionTimer.invalidate();

    m_routeCoordinates.clear();
    emit routeCoordinatesChanged();

    updateRouteGeoJson();
    refreshRouteProjection();

    m_targetZoom = DefaultZoom;

    // Cancel any active overview
    m_routeOverviewActive = false;
    m_overviewTimer->stop();
}

// ---------------------------------------------------------------------------
// Route GeoJSON for native MapLibre layer
// ---------------------------------------------------------------------------

void MapService::updateRouteGeoJson()
{
    QString json;
    if (m_routeShape.isEmpty()) {
        json = QStringLiteral("{\"type\":\"FeatureCollection\",\"features\":[]}");
    } else {
        QStringList coordParts;
        coordParts.reserve(m_routeShape.size());
        for (const auto &pt : m_routeShape) {
            coordParts.append(QStringLiteral("[%1,%2]")
                .arg(pt.second, 0, 'f', 6)   // longitude first (GeoJSON order)
                .arg(pt.first, 0, 'f', 6));   // latitude second
        }
        json = QStringLiteral("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[%1]}}")
            .arg(coordParts.join(QLatin1Char(',')));
    }

    if (json != m_routeGeoJson) {
        m_routeGeoJson = json;
        emit routeGeoJsonChanged();
    }
}

// ---------------------------------------------------------------------------
// GPS position changed
// ---------------------------------------------------------------------------

void MapService::onGpsPositionChanged()
{
    double gpsLat = m_gps->latitude();
    double gpsLng = m_gps->longitude();

    if (gpsLat == 0 && gpsLng == 0)
        return;

    // First valid fix: initialise dead reckoning position
    if (!m_hasInitialPosition) {
        m_hasInitialPosition = true;
        m_drLatitude = gpsLat;
        m_drLongitude = gpsLng;
        m_lastGpsLatitude = gpsLat;
        m_lastGpsLongitude = gpsLng;
        m_gpsErrorLatitude = 0;
        m_gpsErrorLongitude = 0;
        // Seed bearing: prefer the first route segment's bearing when a
        // route is already loaded so the map points the right way while
        // stationary at boot. updateBearing freezes below 1 km/h, so a bad
        // seed from GPS course (typically 0 when stationary) would persist
        // until the rider accelerated past the freeze threshold.
        double seedBearing = m_gps->course();
        if (m_routeShape.size() >= 2) {
            double rb = bearingBetween(m_routeShape[0].first,  m_routeShape[0].second,
                                        m_routeShape[1].first,  m_routeShape[1].second);
            seedBearing = rb;
        }
        m_smoothedTarget = seedBearing;
        m_displayBearing = seedBearing;

        m_mapLatitude = gpsLat;
        m_mapLongitude = gpsLng;

        if (!m_isReady) {
            m_isReady = true;
            emit isReadyChanged();

            // Re-emit route GeoJSON in case it was set before the map
            // GL context was ready (native layers may ignore pre-init data)
            if (!m_routeGeoJson.isEmpty()) {
                emit routeGeoJsonChanged();
            }
        }
        emit mapLatitudeChanged();
        emit mapLongitudeChanged();
        return;
    }

    double ecuSpeedMs = m_engine->speed() * (1000.0 / 3600.0);
    bool stationary = ecuSpeedMs < StationarySpeedMs;

    // Input-side age compensation: the GPS fix represents where the rider
    // WAS some time ago (receiver NMEA buffer + consumer age). Project it
    // forward along the rider's motion direction so subsequent blending
    // pulls DR toward "where GPS thinks we are NOW" rather than backwards
    // in time. On-route we use the route segment bearing — aligned with
    // DR's projectPositionAlongRoute advance and immune to the camera
    // bearing's smoothing lag, which would otherwise inject a sideways
    // component into the error vector and send DR zig-zagging through
    // every turn. Off-route we fall back to the smoothed display bearing.
    if (!stationary) {
        double ageMs = static_cast<double>(m_gps->timestampAgeMs()) + GpsReceiverBufferMs;
        if (ageMs > 0) {
            double motionBearing = m_displayBearing;
            bool onRouteForAge = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0
                                 && m_navigation && m_navigation->isNavigating()
                                 && !m_navigation->isOffRoute();
            if (onRouteForAge) {
                double rb = routeSegmentBearing();
                if (rb >= 0) motionBearing = rb;
            }
            projectForward(gpsLat, gpsLng, motionBearing,
                           ecuSpeedMs * (ageMs / 1000.0),
                           gpsLat, gpsLng);
        }
    }

    // Compute GPS error (distance from DR position to compensated GPS fix)
    double error = haversineDistance(m_drLatitude, m_drLongitude, gpsLat, gpsLng);

    if (error > SnapUpperThreshold) {
        // Extreme error: immediate reset
        m_drLatitude = gpsLat;
        m_drLongitude = gpsLng;
        m_gpsErrorLatitude = 0;
        m_gpsErrorLongitude = 0;
        m_isSnapping = false;
    } else if (!stationary && error > SnapThreshold
               && !(m_navigation && (m_navigation->isOffRoute() || m_navigation->isRerouting()))) {
        // Large jump while on-route: animated snap over SnapAnimationDuration.
        // Off-route / rerouting windows fall through to gradual blending so
        // the marker doesn't bounce between stale-route and real-GPS poses.
        m_isSnapping = true;
        m_snapProgress = 0;
        m_snapStartLat = m_drLatitude;
        m_snapStartLng = m_drLongitude;
        m_snapTargetLat = gpsLat;
        m_snapTargetLng = gpsLng;
    } else if (!stationary) {
        // Normal / small error: accumulate correction for gradual blending
        m_gpsErrorLatitude = gpsLat - m_drLatitude;
        m_gpsErrorLongitude = gpsLng - m_drLongitude;
    }

    m_lastGpsLatitude = gpsLat;
    m_lastGpsLongitude = gpsLng;

    // Off-route -> on-route transition: unlock the HWM so the matcher can
    // land on any segment when the rider re-acquires the route. Without
    // this, an overshoot-and-reverse back to a segment behind the old HWM
    // leaves the matcher stuck on a segment the rider has already left.
    bool nowOffRoute = m_navigation && m_navigation->isOffRoute();
    if (m_lastWasOffRoute && !nowOffRoute)
        m_maxReachedSegment = -1;
    m_lastWasOffRoute = nowOffRoute;

    // Re-evaluate the route segment using trajectory-aware matching: combines
    // perpendicular distance, travel-direction alignment, and hysteresis bias
    // toward the current segment. Prevents the marker from snapping backward
    // on U-turns / sharp turns just because some earlier segment happens to
    // be geometrically closer. Skip while stationary (GPS noise would bounce
    // us between nearby segments) and while off-route (the stale shape isn't
    // meaningful — DR uses straight-line projection during that window).
    if (!stationary && m_routeShape.size() >= 2 &&
        !(m_navigation && (m_navigation->isOffRoute() || m_navigation->isRerouting()))) {
        double speedKmh = m_engine->speed();
        bool haveTrajectory = speedKmh >= MinSpeedForTrajectoryKmh;
        double trajectoryBearing = m_displayBearing;

        SegmentMatch m = matchRouteSegment(gpsLat, gpsLng,
                                            trajectoryBearing, haveTrajectory,
                                            m_currentRouteSegment);
        if (m.index >= 0 && m.perpDist < MatchAcceptanceDistance) {
            bool accept = false;
            if (m_currentRouteSegment < 0 ||
                m_currentRouteSegment >= m_routeShape.size() - 1) {
                // No prior — take whatever matcher suggests
                accept = true;
            } else if (m.index == m_currentRouteSegment) {
                accept = false;  // no change
            } else {
                // Require a meaningful cost improvement to switch segments
                const auto &A = m_routeShape[m_currentRouteSegment];
                const auto &B = m_routeShape[m_currentRouteSegment + 1];
                double sLat, sLng, curDist;
                projectOntoSegment(gpsLat, gpsLng,
                                   A.first, A.second, B.first, B.second,
                                   sLat, sLng, curDist);
                double curCost = curDist - CurrentSegmentBonus;
                if (haveTrajectory) {
                    double segBearing = bearingBetween(A.first, A.second,
                                                        B.first, B.second);
                    double diff = std::abs(signedAngleDiff(trajectoryBearing, segBearing));
                    if (diff > 90.0)
                        curCost += ReverseDirectionPenalty + (diff - 90.0) * ReverseSlopePerDeg;
                    else
                        curCost += diff * SoftDirectionFactor;
                }
                accept = (m.cost + SwitchHysteresis < curCost);
            }
            if (accept) {
                m_currentRouteSegment = m.index;
                m_maxReachedSegment = std::max(m_maxReachedSegment,
                                                m_currentRouteSegment);
            }
        }
    }

    // Refresh projection state exposed to NavigationService
    refreshRouteProjection();

    // Check if GPS position is outside mbtiles bounds
    checkOutOfCoverage();
}

// ---------------------------------------------------------------------------
// Route changed
// ---------------------------------------------------------------------------

void MapService::onRouteChanged()
{
    if (!m_navigation->hasRoute()) {
        clearRoute();
        return;
    }

    updateRouteFromNavigation();

    // Brief zoom-out to give the rider route context
    m_routeOverviewActive = true;
    m_overviewTimer->start();
}

void MapService::onOverviewTimeout()
{
    m_routeOverviewActive = false;
}

// ---------------------------------------------------------------------------
// Theme / map type changed
// ---------------------------------------------------------------------------

void MapService::onThemeChanged()
{
    rebuildStyleUrl();
}

void MapService::onMapTypeChanged()
{
    rebuildStyleUrl();
}

void MapService::onTrafficOverlayChanged()
{
    rebuildStyleUrl();
}

void MapService::removeTrafficFromStyle(QJsonObject &root)
{
    // Remove google-traffic source
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    if (sources.contains(QStringLiteral("google-traffic"))) {
        sources.remove(QStringLiteral("google-traffic"));
        root[QStringLiteral("sources")] = sources;
        qDebug() << "MapService: stripped google-traffic source";
    }

    // Remove traffic-overlay layer
    QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    QJsonArray filtered;
    for (const QJsonValue &v : layers) {
        QJsonObject layer = v.toObject();
        if (layer.value(QStringLiteral("id")).toString() == QStringLiteral("traffic-overlay"))
            continue;
        filtered.append(v);
    }
    root[QStringLiteral("layers")] = filtered;
}

void MapService::rebuildStyleUrl()
{
    bool isDark = m_theme->isDark();
    bool useLocal = !m_mbtilesPath.isEmpty();
    bool showTraffic = m_settings->mapTrafficOverlay();

    qDebug() << "MapService: rebuildStyleUrl - dark:" << isDark
             << "mbtiles:" << (useLocal ? m_mbtilesPath : QStringLiteral("none"))
             << "traffic:" << showTraffic;

    QString qrcPath = isDark
        ? QStringLiteral("qrc:/ScootUI/assets/styles/mapdark.json")
        : QStringLiteral("qrc:/ScootUI/assets/styles/maplight.json");

    QString url;
    if (useLocal) {
        url = rewriteStyleForMbtiles(qrcPath, m_mbtilesPath);
    } else if (!showTraffic) {
        // Online mode with traffic disabled: rewrite style to strip traffic layer
        url = rewriteStyleStripTraffic(qrcPath);
    } else {
        url = qrcPath;
        qDebug() << "MapService: using online style:" << url;
    }

    if (url != m_styleUrl) {
        qDebug() << "MapService: style URL changed:" << url;
        m_styleUrl = url;
        emit styleUrlChanged();
    }
}

QString MapService::rewriteStyleForMbtiles(const QString &qrcPath, const QString &mbtilesPath)
{
    // Determine output path (include traffic state so URL changes when toggled)
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);  // "mapdark.json" or "maplight.json"
    QString stem = baseName.chopped(5);  // strip ".json"
    bool showTraffic = m_settings->mapTrafficOverlay();
    QString outPath = QStringLiteral("/tmp/") + stem
        + (showTraffic ? QStringLiteral("") : QStringLiteral("-notraffic"))
        + QStringLiteral(".json");

    // Read embedded style from QRC
    QString qrcFile = qrcPath;
    qrcFile.replace(QStringLiteral("qrc:/"), QStringLiteral(":/"));
    QFile f(qrcFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "MapService: cannot open embedded style" << qrcFile;
        return qrcPath;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        qWarning() << "MapService: invalid style JSON";
        return qrcPath;
    }

    QJsonObject root = doc.object();

    // Rewrite sources to use mbtiles://
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    for (auto it = sources.begin(); it != sources.end(); ++it) {
        QJsonObject src = it.value().toObject();
        // Only rewrite vector sources; keep raster sources (e.g. traffic overlay) as-is
        if (src.value(QStringLiteral("type")).toString() != QStringLiteral("vector"))
            continue;
        src.remove(QStringLiteral("tiles"));
        QString mbtilesUrl = QStringLiteral("mbtiles://") + mbtilesPath;
        src[QStringLiteral("url")] = mbtilesUrl;
        // Cap maxzoom to actual tile data so MapLibre overzooms correctly
        src[QStringLiteral("maxzoom")] = 14;
        sources[it.key()] = src;
        qDebug() << "MapService: source" << it.key() << "-> " << mbtilesUrl;
    }
    root[QStringLiteral("sources")] = sources;

    // Remove glyphs/sprite URLs (not available offline)
    root.remove(QStringLiteral("glyphs"));
    root.remove(QStringLiteral("sprite"));

    // Remove symbol layers (require remote glyph PBFs)
    QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    QJsonArray filtered;
    for (const QJsonValue &v : layers) {
        QJsonObject layer = v.toObject();
        if (layer.value(QStringLiteral("type")).toString() == QStringLiteral("symbol")) {
            qDebug() << "MapService: stripping symbol layer" << layer.value(QStringLiteral("id")).toString();
            continue;
        }
        filtered.append(v);
    }
    root[QStringLiteral("layers")] = filtered;

    // Strip traffic overlay if disabled
    if (!m_settings->mapTrafficOverlay())
        removeTrafficFromStyle(root);

    // Write to /tmp
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MapService: cannot write" << outPath;
        return qrcPath;
    }
    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    out.write(json);
    out.close();

    QString fileUrl = QStringLiteral("file://") + outPath;
    qDebug() << "MapService: wrote offline style to" << fileUrl << "(" << json.size() << "bytes)";
    return fileUrl;
}

QString MapService::rewriteStyleStripTraffic(const QString &qrcPath)
{
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);
    QString stem = baseName.chopped(5);  // strip ".json"
    QString outPath = QStringLiteral("/tmp/") + stem + QStringLiteral("-notraffic.json");

    QString qrcFile = qrcPath;
    qrcFile.replace(QStringLiteral("qrc:/"), QStringLiteral(":/"));
    QFile f(qrcFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "MapService: cannot open embedded style" << qrcFile;
        return qrcPath;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        qWarning() << "MapService: invalid style JSON";
        return qrcPath;
    }

    QJsonObject root = doc.object();
    removeTrafficFromStyle(root);

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MapService: cannot write" << outPath;
        return qrcPath;
    }
    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    out.write(json);
    out.close();

    QString fileUrl = QStringLiteral("file://") + outPath;
    qDebug() << "MapService: wrote no-traffic style to" << fileUrl << "(" << json.size() << "bytes)";
    return fileUrl;
}

// ---------------------------------------------------------------------------
// Dead reckoning tick (15 Hz)
// ---------------------------------------------------------------------------

void MapService::onDeadReckoningTick()
{
    if (!m_hasInitialPosition || m_deadReckoningPaused)
        return;

    double dtMs = static_cast<double>(m_elapsed.restart());
    double dt = dtMs / 1000.0;

    // Clamp to avoid huge jumps after app resume
    if (dt > 0.5)
        dt = 0.5;

    // ----- Speed (from ECU, km/h -> m/s) -----
    double speedKmh = m_engine->speed();
    double speedMs = speedKmh * (1000.0 / 3600.0);

    // ----- Odometer-primary distance (odometer = truth, speed = feedforward) -----
    // Seed on first tick after we have a map position; odometer deltas from
    // this point on represent cumulative travelled distance. Between odometer
    // edges (100 m quantisation on the ECU) we advance with speed * dt and
    // reconcile via a bounded catchup term so the map marker stays smooth.
    double odoNow = m_engine->odometer();
    if (!m_odoSeeded) {
        if (odoNow > 0 || m_engine->odometer() == 0) {
            // Accept even a zero reading — we just need a baseline.
            m_odoAtSeed = odoNow;
            m_odoTarget = 0;
            m_drTravelled = 0;
            m_odoSeeded = true;
        }
    } else {
        double newTarget = odoNow - m_odoAtSeed;
        if (newTarget < m_odoTarget - 1.0) {
            // Odometer rolled back (reboot, reset, NIL read). Reseed so
            // future deltas stay continuous; don't snap the DR position.
            m_odoAtSeed = odoNow;
            m_odoTarget = 0;
            m_drTravelled = 0;
        } else {
            m_odoTarget = newTarget;
        }
    }

    // ----- Snap animation -----
    if (m_isSnapping) {
        m_snapProgress += dt / SnapAnimationDuration;
        if (m_snapProgress >= 1.0) {
            m_snapProgress = 1.0;
            m_isSnapping = false;
            m_drLatitude = m_snapTargetLat;
            m_drLongitude = m_snapTargetLng;
            m_gpsErrorLatitude = 0;
            m_gpsErrorLongitude = 0;
        } else {
            // Ease-out cubic
            double t = 1.0 - (1.0 - m_snapProgress) * (1.0 - m_snapProgress) * (1.0 - m_snapProgress);
            m_drLatitude = m_snapStartLat + (m_snapTargetLat - m_snapStartLat) * t;
            m_drLongitude = m_snapStartLng + (m_snapTargetLng - m_snapStartLng) * t;
        }
    } else {
        // ----- Project position forward -----
        // Feedforward: speed × dt between odometer edges.
        // Correction:  bounded pull toward odoTarget (ground truth).
        double ffAdvance = speedMs * dt;
        if (speedMs < StationarySpeedMs)
            ffAdvance = 0;  // don't integrate speed noise while stopped

        double distMeters = ffAdvance;
        if (m_odoSeeded) {
            double deficit = m_odoTarget - m_drTravelled;
            double correction = std::clamp(deficit * CatchupRate * dt,
                                            -ffAdvance,            // never rewind
                                            MaxCatchupPerTick);     // never lurch
            distMeters = std::max(0.0, ffAdvance + correction);
            m_drTravelled += distMeters;
        }

        // Route-locked DR (projection + snap) must not use the old route shape
        // while off-route: the stale geometry pulls the marker back and, with
        // globally-nearest segment selection, often *behind* the rider. Until
        // the reroute lands with a new shape, fall back to straight-line DR.
        bool haveRouteShape = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0;
        bool useRouteShape = haveRouteShape && !m_navigation->isOffRoute()
                                             && !m_navigation->isRerouting();

        if (useRouteShape) {
            projectPositionAlongRoute(distMeters);
        } else {
            // Use the smoothed display bearing rather than raw GPS course so DR
            // doesn't drift along a stale heading if the GPS fix is old.
            projectPositionStraight(distMeters, m_displayBearing);
        }

        // ----- GPS correction blending (only when GPS fix is recent) -----
        // While stationary (ECU says we're not moving), skip GPS blending and
        // route snapping — GPS jitter would otherwise slide the marker along
        // the route and snap back when the fix recovers.
        bool stationary = speedMs < StationarySpeedMs;
        if (!stationary && m_gps->hasRecentFix()) {
            blendGpsCorrection(dt);
        }

        // ----- Sticky-route snap state machine -----
        // Decide whether to lock DR to the route line or let it follow GPS,
        // based on a debounced view of m_distFromRoute. Prevents the marker
        // from oscillating between snap and GPS-blend at small deviations.
        // Uses last tick's m_distFromRoute (refreshed at the bottom of the
        // tick) — 67 ms latency is irrelevant against the dwell windows.
        if (useRouteShape)
            evaluateSnapLock();

        // ----- Snap DR back onto the route line after GPS correction -----
        if (!stationary && useRouteShape && m_drLocked) {
            snapToRouteLine();
        }

        // Refresh the projection state NavigationService consumes. Cheap,
        // tracks segment advancement done inside projectPositionAlongRoute
        // as well as blend/snap shifts. Emits routeProjectionChanged only
        // on meaningful change.
        if (haveRouteShape)
            refreshRouteProjection();
    }

    // ----- Update bearing & zoom first (needed for offset calculation) -----
    // Order matters: compensation below projects the marker forward along
    // the current heading. Running updateBearing first means the snap at a
    // segment boundary lands in this tick instead of next, so compensation
    // uses the new-segment direction rather than the previous tick's stale
    // one. Without this swap, the compensation vector flips direction a
    // tick late and the displayed marker jerks sideways through every turn.
    updateBearing(dt);
    updateDynamicZoom(dt);

    // ----- Latency compensation -----
    // Project the displayed position forward to compensate for GPS latency,
    // without modifying the internal DR state. Compensation direction is
    // the rider's actual motion direction. On-route that's the current
    // route segment bearing — rock-steady within a segment, flipping
    // cleanly at waypoint boundaries (same instant DR advances). Off-route
    // or routeless, fall back to the smoothed display bearing.
    double compensationBearing = m_displayBearing;
    bool onRouteForComp = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0
                          && m_navigation->isNavigating()
                          && !m_navigation->isOffRoute();
    if (onRouteForComp) {
        double rb = routeSegmentBearing();
        if (rb >= 0) compensationBearing = rb;
    }
    double compensatedLat = m_drLatitude;
    double compensatedLng = m_drLongitude;
    if (speedMs > 0.5) {
        projectForward(m_drLatitude, m_drLongitude, compensationBearing,
                       speedMs * LatencyCompensationSec,
                       compensatedLat, compensatedLng);
    }

    // ----- Update camera position -----
    // Expose the vehicle position directly; the QML layer uses
    // alignCoordinateToPoint to place the vehicle at the correct screen
    // offset and let Qt handle the bearing-aware pivot calculation.
    if (std::isfinite(compensatedLat) && std::isfinite(compensatedLng)) {
        bool latChanged = (compensatedLat != m_mapLatitude);
        bool lngChanged = (compensatedLng != m_mapLongitude);

        m_mapLatitude = compensatedLat;
        m_mapLongitude = compensatedLng;

        if (latChanged) emit mapLatitudeChanged();
        if (lngChanged) emit mapLongitudeChanged();
    }

    // Notify downstream consumers (e.g. NavigationService for TBT) of the
    // updated DR position. Fires at the 15 Hz tick rate; consumers should
    // throttle if they do expensive work.
    emit vehiclePositionChanged();

    // ----- isReady -----
    if (!m_isReady && m_hasInitialPosition) {
        m_isReady = true;
        emit isReadyChanged();
    }
}

// ---------------------------------------------------------------------------
// Dead reckoning: project along route geometry
// ---------------------------------------------------------------------------

void MapService::projectPositionAlongRoute(double distMeters)
{
    if (m_routeShape.size() < 2 || m_currentRouteSegment < 0)
        return;

    int seg = m_currentRouteSegment;
    double remaining = distMeters;

    while (remaining > 0 && seg < m_routeShape.size() - 1) {
        double nextLat = m_routeShape[seg + 1].first;
        double nextLng = m_routeShape[seg + 1].second;

        double distOnSeg = haversineDistance(m_drLatitude, m_drLongitude, nextLat, nextLng);

        if (remaining >= distOnSeg) {
            // Advance to the end of this segment. When this is the final
            // segment, seg reaches size-1 and the while loop exits; any
            // excess distance past the destination is intentionally
            // discarded so DR doesn't drift past the end of the route.
            m_drLatitude = nextLat;
            m_drLongitude = nextLng;
            remaining -= distOnSeg;
            seg++;
        } else {
            // Interpolate within current segment
            double brng = bearingBetween(m_drLatitude, m_drLongitude, nextLat, nextLng);
            projectForward(m_drLatitude, m_drLongitude, brng, remaining,
                           m_drLatitude, m_drLongitude);
            remaining = 0;
        }
    }

    m_currentRouteSegment = seg;
    m_maxReachedSegment = std::max(m_maxReachedSegment, m_currentRouteSegment);
}

// ---------------------------------------------------------------------------
// Dead reckoning: project straight line
// ---------------------------------------------------------------------------

void MapService::projectPositionStraight(double distMeters, double headingDeg)
{
    if (distMeters < 0.001)
        return;

    projectForward(m_drLatitude, m_drLongitude, headingDeg, distMeters,
                   m_drLatitude, m_drLongitude);
}

// ---------------------------------------------------------------------------
// GPS correction blending
// ---------------------------------------------------------------------------

void MapService::blendGpsCorrection(double dt)
{
    if (std::abs(m_gpsErrorLatitude) < 1e-10 && std::abs(m_gpsErrorLongitude) < 1e-10)
        return;

    // Determine error magnitude to choose blend rate
    double errorDist = haversineDistance(m_drLatitude, m_drLongitude,
                                         m_drLatitude + m_gpsErrorLatitude,
                                         m_drLongitude + m_gpsErrorLongitude);
    double blendRate;
    if (errorDist < LargeErrorThreshold) {
        blendRate = BlendRateNormal;
    } else {
        blendRate = BlendRateLarge;
    }

    double factor = std::min(1.0, blendRate * dt);

    m_drLatitude += m_gpsErrorLatitude * factor;
    m_drLongitude += m_gpsErrorLongitude * factor;

    m_gpsErrorLatitude *= (1.0 - factor);
    m_gpsErrorLongitude *= (1.0 - factor);
}

// ---------------------------------------------------------------------------
// Sticky route snap state machine
// ---------------------------------------------------------------------------

void MapService::evaluateSnapLock()
{
    if (m_routeShape.size() < 2 || m_currentRouteSegment < 0)
        return;
    // Don't perturb in-flight snap animations — they own DR until they finish.
    if (m_isSnapping)
        return;

    if (m_drLocked) {
        // While locked, watch for sustained large cross-track distance. When
        // the rider has been beyond SnapBreakAwayDist for SnapBreakAwayMs the
        // snap releases and DR is allowed to follow GPS via blendGpsCorrection.
        if (m_distFromRoute > SnapBreakAwayDist) {
            if (!m_lockTransitionTimer.isValid()) {
                m_lockTransitionTimer.start();
            } else if (m_lockTransitionTimer.elapsed() >= SnapBreakAwayMs) {
                m_drLocked = false;
                m_lockTransitionTimer.invalidate();
            }
        } else {
            m_lockTransitionTimer.invalidate();
        }
    } else {
        // While unlocked, watch for sustained close cross-track. After
        // SnapReLockDist for SnapReLockMs, kick off a sanfter re-lock — reuse
        // the existing snap-animation pipeline so DR eases onto the segment
        // perpendicular foot over SnapAnimationDuration instead of jumping.
        if (m_distFromRoute < SnapReLockDist) {
            if (!m_lockTransitionTimer.isValid()) {
                m_lockTransitionTimer.start();
            } else if (m_lockTransitionTimer.elapsed() >= SnapReLockMs) {
                m_drLocked = true;
                m_lockTransitionTimer.invalidate();

                if (m_currentRouteSegment + 1 < m_routeShape.size()) {
                    m_isSnapping = true;
                    m_snapProgress = 0;
                    m_snapStartLat = m_drLatitude;
                    m_snapStartLng = m_drLongitude;
                    m_snapTargetLat = m_segmentSnappedLat;
                    m_snapTargetLng = m_segmentSnappedLng;
                }
            }
        } else {
            m_lockTransitionTimer.invalidate();
        }
    }
}

// ---------------------------------------------------------------------------
// Snap DR position onto the current route segment (cross-track correction)
// ---------------------------------------------------------------------------

void MapService::snapToRouteLine()
{
    if (m_routeShape.size() < 2 || m_currentRouteSegment < 0
            || m_currentRouteSegment >= m_routeShape.size() - 1)
        return;

    double aLat = m_routeShape[m_currentRouteSegment].first;
    double aLon = m_routeShape[m_currentRouteSegment].second;
    double bLat = m_routeShape[m_currentRouteSegment + 1].first;
    double bLon = m_routeShape[m_currentRouteSegment + 1].second;

    double cosLat = std::cos(m_drLatitude * DegToRad);
    // Local metric offsets of segment endpoints from DR position
    double ax = (aLon - m_drLongitude) * EarthRadius * cosLat * DegToRad;
    double ay = (aLat - m_drLatitude) * EarthRadius * DegToRad;
    double bx = (bLon - m_drLongitude) * EarthRadius * cosLat * DegToRad;
    double by = (bLat - m_drLatitude) * EarthRadius * DegToRad;
    double dx = bx - ax;
    double dy = by - ay;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-6)
        return;

    // Foot of perpendicular onto the infinite line through A-B. Clamping t
    // to [0, 1] would tuck DR back to the segment endpoint when blend pushes
    // it a hair past B mid-tick — producing the forward-then-step-back jitter
    // at every waypoint crossing. projectPositionAlongRoute advances the
    // segment index on the next tick, so this only needs cross-track pull.
    double t = (-ax * dx - ay * dy) / lenSq;
    double projX = ax + t * dx;  // meters east of DR position
    double projY = ay + t * dy;  // meters north of DR position

    m_drLatitude  += projY / EarthRadius * RadToDeg;
    m_drLongitude += projX / (EarthRadius * cosLat) * RadToDeg;

    // Clear the cross-track component of accumulated GPS error so subsequent
    // blend steps don't immediately pull us off the route line again.
    double errorCosLat = std::cos(m_drLatitude * DegToRad);
    double ex = m_gpsErrorLongitude * EarthRadius * errorCosLat * DegToRad;
    double ey = m_gpsErrorLatitude  * EarthRadius * DegToRad;
    // Keep only the along-track component
    double alongTrackFrac = (ex * dx + ey * dy) / lenSq;
    m_gpsErrorLatitude  = alongTrackFrac * dy / (EarthRadius * DegToRad);
    m_gpsErrorLongitude = alongTrackFrac * dx / (EarthRadius * errorCosLat * DegToRad);
}

// ---------------------------------------------------------------------------
// Dynamic zoom
// ---------------------------------------------------------------------------

void MapService::updateDynamicZoom(double dt)
{
    double effectiveTarget;
    double smoothRate;
    double minClamp;

    if (m_routeOverviewActive) {
        // During overview: zoom out to OverviewZoom at a faster rate
        effectiveTarget = OverviewZoom;
        smoothRate = OverviewZoomRate;
        minClamp = OverviewZoom;
    } else {
        double newTarget = computeTargetZoom();
        if (std::abs(newTarget - m_targetZoom) > ZoomHysteresis) {
            m_targetZoom = newTarget;
        }
        effectiveTarget = m_targetZoom;
        smoothRate = ZoomSmoothRate;
        minClamp = MinZoom;

        // Use faster rate when zooming back in from overview
        if (m_currentZoom < MinZoom)
            smoothRate = OverviewZoomRate;
    }

    // Smooth towards target
    if (std::abs(m_currentZoom - effectiveTarget) > 0.001) {
        double maxStep = smoothRate * dt;
        double diff = effectiveTarget - m_currentZoom;
        double step = std::clamp(diff, -maxStep, maxStep);
        m_currentZoom += step;
        m_currentZoom = std::clamp(m_currentZoom, minClamp, MaxZoom);

        if (m_currentZoom != m_mapZoom && std::isfinite(m_currentZoom)) {
            m_mapZoom = m_currentZoom;
            emit mapZoomChanged();
        }
    }
}

double MapService::computeTargetZoom() const
{
    if (!m_navigation->hasRoute())
        return DefaultZoom;

    const bool rerouting = m_navigation->isRerouting();
    const bool navigating = m_navigation->isNavigating();
    if (!navigating && !rerouting)
        return DefaultZoom;

    // Logarithmic zoom formula:
    //   zoom = MaxZoom - (MaxZoom - MinZoom) * log2(dist / 50) / log2(2000 / 50)
    // Clamped to [MinZoom, MaxZoom]
    constexpr double NearDist = 50.0;
    constexpr double FarDist = 2000.0;
    constexpr double LogRange = 5.3219; // log2(2000/50) ~ log2(40)

    double dist;
    if (rerouting || m_navigation->isOffRoute()) {
        // Off-route / rerouting: frame rider + nearest rejoin point using the
        // perpendicular distance to the route (global-nearest). Distance to
        // next maneuver is meaningless here — the rider isn't tracking toward
        // it — and Rerouting status used to fall through to DefaultZoom.
        dist = m_distFromRoute;
        if (dist <= 0)
            return DefaultZoom;
    } else {
        dist = distanceToNextManeuver();
        if (dist <= 0)
            return DefaultZoom;

        // Multi-turn look-ahead: if a second maneuver is within 150m, use the closer one
        double dist2 = distanceToSecondManeuver();
        if (dist2 > 0 && dist2 < MultiTurnLookAheadMeters) {
            dist = std::min(dist, dist2);
        }
    }

    if (dist <= NearDist)
        return MaxZoom;
    if (dist >= FarDist)
        return MinZoom;

    double ratio = std::log2(dist / NearDist) / LogRange;
    double zoom = MaxZoom - (MaxZoom - MinZoom) * ratio;
    return std::clamp(zoom, MinZoom, MaxZoom);
}

double MapService::distanceToNextManeuver() const
{
    return m_navigation->currentManeuverDistance();
}

double MapService::distanceToSecondManeuver() const
{
    if (!m_navigation->hasNextInstruction())
        return -1.0;
    return m_navigation->nextManeuverDistance();
}

// ---------------------------------------------------------------------------
// Rotation smoothing
// ---------------------------------------------------------------------------

double MapService::routeSegmentBearing() const
{
    if (m_routeShape.size() < 2 || m_currentRouteSegment < 0
        || m_currentRouteSegment >= m_routeShape.size() - 1)
        return -1;

    int seg = m_currentRouteSegment;
    return bearingBetween(m_routeShape[seg].first, m_routeShape[seg].second,
                          m_routeShape[seg + 1].first, m_routeShape[seg + 1].second);
}

void MapService::updateBearing(double dt)
{
    double speedKmh = m_engine->speed();

    // m_drLocked couples rotation to the sticky-snap lock state — once the
    // marker starts following GPS (break-away), the map rotation switches to
    // GPS course too, so the two move together. Without this, a deviation
    // between 12m (break-away) and 60m (off-route flag) leaves the map
    // rotating to the stale route bearing while the marker drifts off it,
    // which reads as the marker sliding sideways or backwards.
    bool onRoute = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0
                   && m_navigation->isNavigating()
                   && !m_navigation->isOffRoute()
                   && m_drLocked;
    bool hasFix = m_gps->hasRecentFix();
    double gpsCourse = m_gps->course();

    // Off-route with a stale GPS fix: nothing to steer by. Hold the last
    // smoothed bearing rather than snapping to a stale gpsCourse.
    if (!onRoute && !hasFix)
        return;

    double rawHeading;
    if (onRoute && !hasFix) {
        // DR on route: use route bearing only
        double rb = routeSegmentBearing();
        rawHeading = (rb >= 0) ? rb : gpsCourse;
    } else if (onRoute && hasFix) {
        // On-route: route geometry is authoritative. gpsCourse lags through
        // turns (reported ≤1 Hz; between updates it still shows the pre-turn
        // direction), and any blend with the stale course dragged
        // m_smoothedTarget back toward the old bearing for a beat after a
        // turn-snap — the map would swing forward, bounce partway back, then
        // settle. If the rider genuinely deviates, off-route detection kicks
        // in and the else branch takes over with pure gpsCourse.
        double rb = routeSegmentBearing();
        rawHeading = (rb >= 0) ? rb : gpsCourse;
    } else {
        // No route: blend GPS course with road bearing from vector tiles
        double rb = m_speedLimit->roadBearing();
        if (rb >= 0 && hasFix) {
            // Road bearing is directionless — pick the direction closest to GPS course
            double diff = normalizeAngle(rb - gpsCourse);
            if (std::abs(diff) > 90.0)
                rb = std::fmod(rb + 180.0, 360.0);
            diff = normalizeAngle(rb - gpsCourse);
            rawHeading = gpsCourse + diff * 0.3;
        } else {
            rawHeading = gpsCourse;
        }
    }

    // Speed-based damping factor: freeze below HeadingFreezeSpeed, ramp to full
    double dampFactor = 0;
    if (speedKmh >= HeadingFullSpeed) {
        dampFactor = 1.0;
    } else if (speedKmh > HeadingFreezeSpeed) {
        dampFactor = (speedKmh - HeadingFreezeSpeed) / (HeadingFullSpeed - HeadingFreezeSpeed);
    }

    if (dampFactor < 0.001) {
        // Heading frozen: don't update
        return;
    }

    // Turn-snap: the route segment bearing jumped (segment boundary with a
    // real corner). Skip stage 1's speed-weighted exponential blend — just
    // jump the target to the new segment so stage 2 below can animate
    // m_displayBearing toward it at MaxBearingRate. ~0.8 s for a 90° turn,
    // still noticeably snappy but no longer a hard camera cut.
    double rbNow = onRoute ? routeSegmentBearing() : -1;
    bool justSnapped = false;
    if (onRoute && rbNow >= 0 && m_lastRouteBearing >= 0) {
        double bearingJump = std::abs(normalizeAngle(rbNow - m_lastRouteBearing));
        if (bearingJump >= TurnSnapDeltaDeg) {
            m_smoothedTarget = std::fmod(rbNow + 360.0, 360.0);
            justSnapped = true;
        }
    }
    m_lastRouteBearing = rbNow;

    // Stage 1: exponential blend toward target (matches Flutter).
    // Skip when we just turn-snapped this tick so m_smoothedTarget doesn't
    // immediately get dragged back toward rawHeading (which still reflects
    // the blended pre-turn gpsCourse for a beat).
    if (!justSnapped) {
        double targetDelta = normalizeAngle(rawHeading - m_smoothedTarget);
        // Each frame moves a proportion of remaining distance, not a fixed step
        double targetBlend = std::min(1.0, TargetSmoothRate * dt * dampFactor);
        m_smoothedTarget += targetDelta * targetBlend;
        m_smoothedTarget = std::fmod(m_smoothedTarget + 360.0, 360.0);
    }

    // Stage 2: duration-based interpolation (matches Flutter)
    // Tries to complete rotation in RotationAnimDuration seconds, capped at MaxBearingRate
    double displayDelta = normalizeAngle(m_smoothedTarget - m_displayBearing);
    double absDelta = std::abs(displayDelta);
    double rotationRate = (absDelta <= MaxBearingRate)
        ? absDelta / RotationAnimDuration
        : MaxBearingRate;

    double rotationStep = rotationRate * dt;
    if (absDelta <= rotationStep || rotationRate == 0) {
        m_displayBearing = m_smoothedTarget;
    } else {
        m_displayBearing += std::copysign(rotationStep, displayDelta);
    }
    m_displayBearing = std::fmod(m_displayBearing + 360.0, 360.0);

    if (m_displayBearing != m_mapBearing && std::isfinite(m_displayBearing)) {
        m_mapBearing = m_displayBearing;
        emit mapBearingChanged();
    }

    m_lastRawHeading = rawHeading;
}

double MapService::normalizeAngle(double angle)
{
    angle = std::fmod(angle, 360.0);
    if (angle > 180.0) angle -= 360.0;
    if (angle < -180.0) angle += 360.0;
    return angle;
}

// ---------------------------------------------------------------------------
// Trajectory-aware segment matcher
// ---------------------------------------------------------------------------

MapService::SegmentMatch MapService::matchRouteSegment(double lat, double lng,
                                                        double trajectoryBearing,
                                                        bool haveTrajectory,
                                                        int currentSegment) const
{
    SegmentMatch best;
    const int n = m_routeShape.size();
    if (n < 2)
        return best;

    int lo, hi;
    if (currentSegment < 0 || currentSegment >= n - 1) {
        // Cold start or segment state lost — full scan, but only accept
        // results within MatchAcceptanceDistance (prevents a random far
        // match when off-route).
        lo = 0;
        hi = n - 1;
    } else {
        lo = std::max(0, currentSegment - MatchWindowBack);
        hi = std::min(n - 1, currentSegment + MatchWindowFwd + 1);
    }

    // High-water mark gate: once the rider has reached a segment, we don't
    // regress below it. Mid-turn the previous segment's endpoint is often
    // geometrically closest; without this gate the matcher would snap back.
    // Only relevant once we have a non-negative HWM (i.e. matched at least
    // once since the route was loaded).
    if (m_maxReachedSegment >= 0)
        lo = std::max(lo, m_maxReachedSegment);
    if (lo >= hi)
        return best;

    for (int i = lo; i < hi; ++i) {
        const auto &A = m_routeShape[i];
        const auto &B = m_routeShape[i + 1];

        double sLat, sLng, perpDist;
        projectOntoSegment(lat, lng, A.first, A.second, B.first, B.second,
                           sLat, sLng, perpDist);

        double cost = perpDist;

        if (haveTrajectory) {
            double segBearing = bearingBetween(A.first, A.second, B.first, B.second);
            double diff = std::abs(signedAngleDiff(trajectoryBearing, segBearing));
            if (diff > 90.0) {
                cost += ReverseDirectionPenalty + (diff - 90.0) * ReverseSlopePerDeg;
            } else {
                cost += diff * SoftDirectionFactor;
            }
        }

        if (currentSegment >= 0) {
            int delta = i - currentSegment;
            if (delta == 0) {
                cost -= CurrentSegmentBonus;
            } else if (delta < 0) {
                cost += (-delta) * BackwardStepPenalty;
            } else {
                cost += delta * ForwardStepPenalty;
            }
        }

        if (best.index < 0 || cost < best.cost) {
            best.index = i;
            best.cost = cost;
            best.perpDist = perpDist;
            best.snappedLat = sLat;
            best.snappedLng = sLng;
        }
    }

    return best;
}

void MapService::refreshRouteProjection()
{
    if (m_routeShape.size() < 2) {
        // No route — clear projection
        bool changed = (m_lastEmittedSegment != -1 ||
                        m_lastEmittedDistFromRoute != 0 ||
                        m_segmentSnappedLat != 0 ||
                        m_segmentSnappedLng != 0);
        m_snappedLat = 0;
        m_snappedLng = 0;
        m_distFromRoute = 0;
        m_segmentSnappedLat = 0;
        m_segmentSnappedLng = 0;
        if (changed) {
            m_lastEmittedSegment = -1;
            m_lastEmittedSnapLat = 0;
            m_lastEmittedSnapLng = 0;
            m_lastEmittedDistFromRoute = 0;
            emit routeProjectionChanged();
        }
        return;
    }

    // distFromRoute / snappedPos are true-nearest-to-any-segment, NOT
    // projection onto m_currentRouteSegment. The matcher's segment pick is a
    // directional/identity concept (which leg of the route are we "on"); the
    // perpendicular distance is a pure-geometry concept. Keeping them
    // independent means off-route recovery still works when the rider
    // rejoins the route at a different segment than where they left —
    // otherwise distFromRoute would stay large (stuck projecting onto the
    // frozen pre-off-route segment) and isOffRoute hysteresis never clears.
    double sLat = m_drLatitude, sLng = m_drLongitude;
    double dist = std::numeric_limits<double>::max();
    for (int i = 0; i < m_routeShape.size() - 1; ++i) {
        const auto &A = m_routeShape[i];
        const auto &B = m_routeShape[i + 1];
        double candLat, candLng, candDist;
        projectOntoSegment(m_drLatitude, m_drLongitude,
                           A.first, A.second, B.first, B.second,
                           candLat, candLng, candDist);
        if (candDist < dist) {
            dist = candDist;
            sLat = candLat;
            sLng = candLng;
        }
    }

    m_snappedLat = sLat;
    m_snappedLng = sLng;
    m_distFromRoute = dist;

    // Segment-aligned projection: snap DR position onto the matcher's current
    // segment specifically (not the geometrically nearest). NavigationService
    // uses this for the along-route walker so that TBT distance-to-next-turn
    // stays consistent with the segment index even when the matcher and the
    // global-nearest disagree (HWM gate, direction penalty, post-reroute).
    if (m_currentRouteSegment >= 0 &&
        m_currentRouteSegment + 1 < m_routeShape.size()) {
        const auto &A = m_routeShape[m_currentRouteSegment];
        const auto &B = m_routeShape[m_currentRouteSegment + 1];
        double segLat, segLng, segDist;
        projectOntoSegment(m_drLatitude, m_drLongitude,
                           A.first, A.second, B.first, B.second,
                           segLat, segLng, segDist);
        m_segmentSnappedLat = segLat;
        m_segmentSnappedLng = segLng;
        (void)segDist;
    } else {
        m_segmentSnappedLat = m_drLatitude;
        m_segmentSnappedLng = m_drLongitude;
    }

    // Emit only if the change is meaningful (segment change or snap-pos
    // moved > SnappedPosEpsilon meters or distFromRoute shifted > epsilon).
    bool segChanged = (m_currentRouteSegment != m_lastEmittedSegment);
    double dLat = sLat - m_lastEmittedSnapLat;
    double dLng = sLng - m_lastEmittedSnapLng;
    double moved = haversineDistance(m_lastEmittedSnapLat, m_lastEmittedSnapLng, sLat, sLng);
    bool posChanged = moved > SnappedPosEpsilon;
    bool distChanged = std::abs(dist - m_lastEmittedDistFromRoute) > SnappedPosEpsilon;

    if (segChanged || posChanged || distChanged) {
        m_lastEmittedSegment = m_currentRouteSegment;
        m_lastEmittedSnapLat = sLat;
        m_lastEmittedSnapLng = sLng;
        m_lastEmittedDistFromRoute = dist;
        emit routeProjectionChanged();
    }
    (void)dLat; (void)dLng;
}

// ---------------------------------------------------------------------------
// Out-of-coverage detection (mbtiles bounds)
// ---------------------------------------------------------------------------

void MapService::loadMbtilesBounds()
{
    if (m_mbtilesPath.isEmpty()) {
        qDebug() << "MapService: no mbtiles file, skipping bounds load";
        return;
    }

    // Use a unique connection name to avoid conflicts
    const QString connName = QStringLiteral("mapservice_bounds");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(m_mbtilesPath);
        if (!db.open()) {
            qWarning() << "MapService: cannot open mbtiles for bounds:" << db.lastError().text();
            QSqlDatabase::removeDatabase(connName);
            return;
        }

        QSqlQuery query(db);
        if (query.exec(QStringLiteral("SELECT value FROM metadata WHERE name='bounds'"))) {
            if (query.next()) {
                // bounds format: "minLng,minLat,maxLng,maxLat"
                QString boundsStr = query.value(0).toString();
                QStringList parts = boundsStr.split(QLatin1Char(','));
                if (parts.size() == 4) {
                    m_boundsMinLng = parts[0].toDouble();
                    m_boundsMinLat = parts[1].toDouble();
                    m_boundsMaxLng = parts[2].toDouble();
                    m_boundsMaxLat = parts[3].toDouble();
                    m_hasBounds = true;
                    qDebug() << "MapService: mbtiles bounds loaded:"
                             << m_boundsMinLat << m_boundsMinLng
                             << m_boundsMaxLat << m_boundsMaxLng;
                }
            }
        } else {
            qWarning() << "MapService: bounds query failed:" << query.lastError().text();
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
}

void MapService::checkOutOfCoverage()
{
    if (!m_hasBounds || !m_hasInitialPosition)
        return;

    double lat = m_lastGpsLatitude;
    double lng = m_lastGpsLongitude;

    bool outOfCoverage = lng < m_boundsMinLng || lng > m_boundsMaxLng ||
                         lat < m_boundsMinLat || lat > m_boundsMaxLat;

    if (outOfCoverage != m_isOutOfCoverage) {
        m_isOutOfCoverage = outOfCoverage;
        emit isOutOfCoverageChanged();
    }
}
