#include "MapService.h"
#include "MapCameraPolicy.h"
#include "RoadInfoService.h"
#include "NavigationService.h"
#include "stores/GpsStore.h"
#include "stores/EngineStore.h"
#include "stores/SettingsStore.h"
#include "stores/ThemeStore.h"
#include "stores/SpeedLimitStore.h"
#include "stores/MotionStore.h"
#include "routing/RouteModels.h"
#include "models/Enums.h"

#include <QDebug>
#include <QVariantMap>
#include <QtMath>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QRegularExpression>
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

// Same local ENU frame as projectOntoSegment, but reports the UNCLAMPED
// along-segment parameter and the segment length. t < 0 means the point sits
// before A, t > 1 that it sits past B; the overshoot in metres is
// (t - 1) * length. Returns false for a degenerate segment.
static bool segmentParam(double lat, double lng,
                         double aLat, double aLon,
                         double bLat, double bLon,
                         double &tOut, double &lengthOut)
{
    const double cosLat = std::cos(lat * DegToRad);
    const double ax = (aLon - lng) * EarthRadius * cosLat * DegToRad;
    const double ay = (aLat - lat) * EarthRadius * DegToRad;
    const double bx = (bLon - lng) * EarthRadius * cosLat * DegToRad;
    const double by = (bLat - lat) * EarthRadius * DegToRad;
    const double dx = bx - ax;
    const double dy = by - ay;
    const double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-6)
        return false;
    tOut = (-ax * dx - ay * dy) / lenSq;
    lengthOut = std::sqrt(lenSq);
    return true;
}

// Point `offset` metres along the polyline from vertex `fromIndex`, forward for
// a positive offset and backward for a negative one. Clamps at either end.
static void walkPolyline(const QList<QPair<double, double>> &shape, int fromIndex,
                         double offset, double &outLat, double &outLng,
                         int *outSegment = nullptr)
{
    const int n = shape.size();
    fromIndex = std::clamp(fromIndex, 0, n - 1);
    outLat = shape[fromIndex].first;
    outLng = shape[fromIndex].second;
    if (outSegment)
        *outSegment = std::clamp(fromIndex, 0, std::max(0, n - 2));
    if (n < 2 || !std::isfinite(offset))
        return;

    double remaining = std::abs(offset);
    const int step = offset >= 0.0 ? 1 : -1;
    int i = fromIndex;
    while (remaining > 0.0) {
        const int j = i + step;
        if (j < 0 || j >= n)
            return;
        const double segLen = haversineDistance(shape[i].first, shape[i].second,
                                                shape[j].first, shape[j].second);
        if (segLen <= 1e-9) {
            i = j;
            continue;
        }
        if (outSegment)
            *outSegment = std::min(i, j);
        if (remaining <= segLen) {
            const double f = remaining / segLen;
            outLat = shape[i].first + (shape[j].first - shape[i].first) * f;
            outLng = shape[i].second + (shape[j].second - shape[i].second) * f;
            return;
        }
        remaining -= segLen;
        i = j;
        outLat = shape[i].first;
        outLng = shape[i].second;
    }
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
                       MotionStore *motion, QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_engine(engine)
    , m_navigation(navigation)
    , m_settings(settings)
    , m_theme(theme)
    , m_speedLimit(speedLimit)
    , m_motion(motion)
    , m_tickTimer(new QTimer(this))
{
    // reloadMbtiles() opens a SQLite connection for the mbtiles validation
    // probe and parses two JSON style files to rewrite tile:// URLs.
    // buildThemeLayerOverrides() parses two more QRC style JSONs (~150ms on
    // iMX6). None of it is needed before the first frame (the map isn't the
    // initial screen), so queue both on the first event-loop tick to keep them
    // off the createStores()/engine.load() critical path.
    QTimer::singleShot(0, this, [this]() {
        buildThemeLayerOverrides();
        reloadMbtiles();
    });

    // --- GPS position updates ---
    connect(m_gps, &GpsStore::sampleChanged, this, &MapService::onGpsPositionChanged);

    // --- Magnetic heading freshness ---
    // Restart the age timer on every motion:heading push so updateBearing can
    // tell whether the compass reading is recent enough to steer the map by.
    if (m_motion)
        connect(m_motion, &MotionStore::headingChanged, this, [this]() { m_headingAge.restart(); });

    // --- Route changes ---
    connect(m_navigation, &NavigationService::routeChanged, this, &MapService::onRouteChanged);

    // --- Theme changes ---
    // No style reload on theme switch: the map QML recolors existing layers in
    // place from m_mapThemeLayers (built above on the first event-loop tick),
    // so the style URL is theme-independent.

    // --- Map type changes (online / offline) ---
    connect(m_settings, &SettingsStore::mapTypeChanged, this, &MapService::onMapTypeChanged);

    // --- View mode / orientation changes (3D ↔ 2D, north ↔ heading) ---
    // The effective bearing and vehicle offset depend on these; see mapBearing()/
    // vehicleOffsetY(). Re-sync nothing else — tilt lives in the QML layer.
    connect(m_settings, &SettingsStore::mapViewModeChanged, this, &MapService::onMapViewModeChanged);
    connect(m_settings, &SettingsStore::mapNorthOrientedChanged, this, [this]() {
        m_northOriented = m_settings->mapNorthOriented();
        // Re-emit so the map/north indicator re-read the (now 0) effective bearing.
        emit mapBearingChanged();
        // The 2D marker offset is gated on the same flag, and the camera aligns
        // the vehicle coordinate to that offset, so both have to be told. Fires
        // only when the rider flips the setting, not continuously.
        emit vehicleOffsetYChanged();
    });

    // Mirror the current settings so a persisted 2D/north-oriented selection is
    // honored even though the *Changed signals above only fire on later changes.
    m_view2D = m_settings->mapViewMode() == static_cast<int>(ScootEnums::MapViewMode::View2D);
    m_northOriented = m_settings->mapNorthOriented();

    // --- Traffic overlay toggle ---
    connect(m_settings, &SettingsStore::mapTrafficOverlayChanged, this, &MapService::onTrafficOverlayChanged);
    // --- Route overview timer (single-shot) ---
    m_overviewTimer = new QTimer(this);
    m_overviewTimer->setSingleShot(true);
    m_overviewTimer->setInterval(OverviewHoldMs);
    connect(m_overviewTimer, &QTimer::timeout, this, &MapService::onOverviewTimeout);

    // --- Dead reckoning and render timer at 20 Hz ---
    m_tickTimer->setTimerType(Qt::PreciseTimer);
    m_tickTimer->setInterval(TickIntervalMs);
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

    QDateTime newMtime = newPath.isEmpty() ? QDateTime() : QFileInfo(newPath).lastModified();

    // Path alone isn't enough: an OTA install renames a new map.mbtiles over
    // the same path, so also check whether the file itself changed.
    if (newPath == m_mbtilesPath && newMtime == m_mbtilesMtime)
        return;

    if (newPath.isEmpty()) {
        qDebug() << "MapService: no mbtiles found, using online tiles";
    } else {
        qDebug() << "MapService: mbtiles loaded:" << newPath;
    }

    m_mbtilesPath = newPath;
    m_mbtilesMtime = newMtime;
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
        double speedKmh = effectiveSpeedKmh();
        bool haveTrajectory = speedKmh >= MinSpeedForTrajectoryKmh;
        SegmentMatch seeded = matchRouteSegment(m_drLatitude, m_drLongitude,
                                                 m_gps->course(), haveTrajectory,
                                                 -1);
        m_currentRouteSegment = (seeded.index >= 0 &&
                                  seeded.perpDist < MatchAcceptanceDistance)
                                ? seeded.index : 0;
    } else {
        m_currentRouteSegment = 0;
    }
    m_maxReachedSegment = m_currentRouteSegment;
    m_lastRouteBearing = -1;
    // NavigationService immediately evaluates the independent physical
    // distance after routeChanged; its on/off-route result is the sole owner of
    // whether presentation follows this route.
    m_lastWasOffRoute = false;
    m_projectionCadence.reset();

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
    m_lastWasOffRoute = false;
    m_projectionCadence.reset();

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
    const GpsSample sample = m_gps->currentSample();
    if (!sample.hasValidCoordinate())
        return;

    double gpsLat = sample.latitude;
    double gpsLng = sample.longitude;

    // Do not seed the estimator from a last-known coordinate accompanying a
    // searching/error snapshot. Once seeded, that coordinate may remain a
    // useful fallback, but startup needs an actual fix.
    if (!m_hasInitialPosition) {
        if (!sample.hasFix())
            return;
        m_hasInitialPosition = true;
        m_drLatitude = gpsLat;
        m_drLongitude = gpsLng;
        m_positionUncertaintyMeters = sample.ephMeters > 0.0
            ? std::clamp(sample.ephMeters, 3.0, MaxEstimatorEphMeters)
            : DefaultGpsUncertaintyMeters;
        m_lastGpsLatitude = gpsLat;
        m_lastGpsLongitude = gpsLng;
        m_gpsErrorLatitude = 0;
        m_gpsErrorLongitude = 0;
        // Seed bearing: prefer the first route segment's bearing when a
        // route is already loaded so the map points the right way while
        // stationary at boot. updateBearing freezes below 1 km/h, so a bad
        // seed from GPS course (typically 0 when stationary) would persist
        // until the rider accelerated past the freeze threshold.
        double seedBearing = sample.course;
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

    double speedMs = effectiveSpeedKmh() * (1000.0 / 3600.0);
    bool stationary = speedMs < StationarySpeedMs;
    const bool accurateEnough = sample.hasAcceptableAccuracy(MaxEstimatorEphMeters);
    const bool recentFix = m_gps->hasRecentFix();

    // Input-side age compensation: the GPS fix represents where the rider
    // WAS some time ago (receiver NMEA buffer + consumer age). Project it
    // forward along the rider's motion direction so subsequent blending
    // pulls DR toward "where GPS thinks we are NOW" rather than backwards
    // in time. On-route we use the route segment bearing — aligned with the
    // estimator's motion model and immune to the camera bearing's smoothing
    // lag, which would otherwise inject a sideways
    // component into the error vector and send DR zig-zagging through
    // every turn. Off-route we fall back to the smoothed display bearing. Only
    // compensate genuinely fresh samples; projecting a many-seconds-old fix
    // along the current heading creates a confident but fictitious position.
    if (!stationary && recentFix && accurateEnough) {
        double ageMs = static_cast<double>(m_gps->timestampAgeMs()) + GpsReceiverBufferMs;
        if (ageMs > 0 && ageMs <= MaxGpsProjectionAgeMs) {
            double motionBearing = m_displayBearing;
            bool onRouteForAge = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0
                                 && m_navigation && m_navigation->isNavigating()
                                 && !m_navigation->isOffRoute();
            if (onRouteForAge) {
                double rb = routeSegmentBearing();
                if (rb >= 0) motionBearing = rb;
            }
            projectForward(gpsLat, gpsLng, motionBearing,
                           speedMs * (ageMs / 1000.0),
                           gpsLat, gpsLng);
        }
    }

    if (recentFix && accurateEnough) {
        const double error = haversineDistance(m_drLatitude, m_drLongitude,
                                               gpsLat, gpsLng);
        const double measurementUncertainty = sample.ephMeters > 0.0
            ? std::clamp(sample.ephMeters, 3.0, MaxEstimatorEphMeters)
            : DefaultGpsUncertaintyMeters;
        m_positionUncertaintyMeters = std::max(
            measurementUncertainty,
            std::min(error, MaxPositionUncertaintyMeters));
        if (error > SnapUpperThreshold) {
            // A physically impossible estimator error is safer to reset than
            // to spend minutes recovering. This never implies a route snap.
            m_drLatitude = gpsLat;
            m_drLongitude = gpsLng;
            m_gpsErrorLatitude = 0;
            m_gpsErrorLongitude = 0;
        } else {
            // Physical estimate always follows the measurement, even at a
            // stop. The tick applies a very low stationary gain so stable fixes
            // can correct a bad parked position without following raw jitter.
            m_gpsErrorLatitude = gpsLat - m_drLatitude;
            m_gpsErrorLongitude = gpsLng - m_drLongitude;
        }
    }

    m_lastGpsLatitude = gpsLat;
    m_lastGpsLongitude = gpsLng;

    if (!stationary && recentFix && accurateEnough && m_routeShape.size() >= 2 &&
        !(m_navigation && (m_navigation->isOffRoute() || m_navigation->isRerouting()))) {
        updateRouteMatch(gpsLat, gpsLng, sample.course,
                         sample.speedKmh >= MinSpeedForTrajectoryKmh);
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

    // Initial routes get a brief extent-aware overview. A reroute must not
    // repeatedly pull the camera away from the rider; its new geometry is
    // already evaluated against the physical pose on the next estimator tick.
    if (!m_navigation->lastRouteWasReroute()) {
        m_overviewZoom = MapCameraPolicy::routeOverviewZoom(
            m_navigation->routeWaypoints(), OverviewMinZoom, OverviewMaxZoom);
        m_routeOverviewActive = true;
        m_overviewTimer->start();
    } else {
        m_routeOverviewActive = false;
        m_overviewTimer->stop();
    }
}

void MapService::onOverviewTimeout()
{
    m_routeOverviewActive = false;
}

// ---------------------------------------------------------------------------
// Theme / map type changed
// ---------------------------------------------------------------------------

void MapService::onMapTypeChanged()
{
    rebuildStyleUrl();
}

void MapService::onMapViewModeChanged()
{
    m_view2D = m_settings->mapViewMode() == static_cast<int>(ScootEnums::MapViewMode::View2D);
    // In 2D the vehicle sits at screen center (no forward tilt offset).
    emit vehicleOffsetYChanged();
    // North-oriented only applies to the 2D view; re-evaluate the effective bearing.
    emit mapBearingChanged();
    // Buildings are extruded in 3D and flat in 2D, which is a different style.
    // Rebuild the theme overrides first so they carry the matching layer type,
    // then the URL, whose change reloads the map.
    buildThemeLayerOverrides();
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

void MapService::buildThemeLayerOverrides()
{
    auto loadLayers = [](const QString &qrcPath) -> QHash<QString, QJsonObject> {
        QHash<QString, QJsonObject> out;
        QString file = qrcPath;
        file.replace(QStringLiteral("qrc:/"), QStringLiteral(":/"));
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "MapService: cannot open style for theme overrides" << file;
            return out;
        }
        const QJsonArray layers = QJsonDocument::fromJson(f.readAll())
                                      .object().value(QStringLiteral("layers")).toArray();
        f.close();
        for (const QJsonValue &v : layers) {
            const QJsonObject layer = v.toObject();
            out.insert(layer.value(QStringLiteral("id")).toString(), layer);
        }
        return out;
    };

    QHash<QString, QJsonObject> dark =
        loadLayers(QStringLiteral("qrc:/ScootUI/assets/styles/mapdark.json"));
    QHash<QString, QJsonObject> light =
        loadLayers(QStringLiteral("qrc:/ScootUI/assets/styles/maplight.json"));

    // In 2D the emitted style carries buildings as plain fills, so the
    // overrides have to be derived from the flattened layers. Otherwise they
    // would keep naming fill-extrusion-color on a layer that is now a fill and
    // the recolour would silently stop applying on a theme change.
    if (m_view2D) {
        for (auto it = dark.begin(); it != dark.end(); ++it)
            it.value() = flattenExtrusionLayer(it.value());
        for (auto it = light.begin(); it != light.end(); ++it)
            it.value() = flattenExtrusionLayer(it.value());
    }

    // Walk the light style's layer order so the overrides keep style order.
    QFile lf(QStringLiteral(":/ScootUI/assets/styles/maplight.json"));
    QJsonArray order;
    if (lf.open(QIODevice::ReadOnly)) {
        order = QJsonDocument::fromJson(lf.readAll())
                    .object().value(QStringLiteral("layers")).toArray();
        lf.close();
    }

    m_mapThemeLayers.clear();
    for (const QJsonValue &v : order) {
        const QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (!dark.contains(id) || !light.contains(id))
            continue;

        const QJsonObject dPaint = dark.value(id).value(QStringLiteral("paint")).toObject();
        const QJsonObject lPaint = light.value(id).value(QStringLiteral("paint")).toObject();

        QStringList keys = dPaint.keys();
        for (const QString &k : lPaint.keys())
            if (!keys.contains(k))
                keys.append(k);

        QVariantMap paintDark, paintLight;
        for (const QString &k : keys) {
            if (dPaint.value(k) == lPaint.value(k))
                continue; // identical between themes: no override needed
            paintDark.insert(k, dPaint.value(k).toVariant());
            paintLight.insert(k, lPaint.value(k).toVariant());
        }
        if (paintDark.isEmpty())
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("styleId"), id);
        entry.insert(QStringLiteral("type"),
                     light.value(id).value(QStringLiteral("type")).toString());
        entry.insert(QStringLiteral("paintDark"), paintDark);
        entry.insert(QStringLiteral("paintLight"), paintLight);
        m_mapThemeLayers.append(entry);
    }

    qDebug() << "MapService: built" << m_mapThemeLayers.size() << "theme layer overrides";
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
    } else {
        // Always materialize the online style. Besides traffic/2D variants,
        // this guarantees the native route source and correctly ordered route
        // layers exist in the normal traffic-enabled 3D configuration too.
        url = rewriteStyleVariant(qrcPath);
    }

    if (url != m_styleUrl) {
        qDebug() << "MapService: style URL changed:" << url;
        m_styleUrl = url;
        emit styleUrlChanged();
    }
}

QString MapService::rewriteStyleForMbtiles(const QString &qrcPath, const QString &mbtilesPath)
{
    // Determine output path (include traffic state + mbtiles mtime so the URL
    // changes whenever traffic is toggled or the mbtiles file is replaced by
    // an OTA install — an unchanged styleUrl string would otherwise suppress
    // styleUrlChanged and leave MapViewWidget rendering the stale map).
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);  // "mapdark.json" or "maplight.json"
    QString stem = baseName.chopped(5);  // strip ".json"
    QString filePrefix = stem + styleVariantSuffix();
    qint64 mtimeSecs = QFileInfo(mbtilesPath).lastModified().toSecsSinceEpoch();
    QString outPath = QStringLiteral("/tmp/") + filePrefix + QStringLiteral("-")
        + QString::number(mtimeSecs) + QStringLiteral(".json");

    // Best-effort cleanup of stale rewritten styles for this stem/traffic
    // combination so /tmp doesn't accumulate one file per map update.
    QDir tmpDir(QStringLiteral("/tmp"));
    const QRegularExpression staleRe(QStringLiteral("^") + QRegularExpression::escape(filePrefix)
                                      + QStringLiteral("-\\d+\\.json$"));
    for (const QString &name : tmpDir.entryList(QDir::Files)) {
        if (staleRe.match(name).hasMatch())
            QFile::remove(tmpDir.filePath(name));
    }

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

    // Sprites are still remote-only, so those go regardless
    root.remove(QStringLiteral("sprite"));

    // Symbol layers need glyph PBFs. A glyph fetch that fails leaves the request
    // unparsed in MapLibre's glyph manager, and the tile then waits on it
    // forever, so every feature on that tile disappears rather than just the
    // label. Only keep the symbol layers when the glyphs are actually installed.
    const QString glyphDir = localGlyphDirectory();
    if (glyphDir.isEmpty()) {
        root.remove(QStringLiteral("glyphs"));
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
    } else {
        const QString glyphUrl =
            QUrl::fromLocalFile(glyphDir).toString() + QStringLiteral("/{fontstack}/{range}.pbf");
        root[QStringLiteral("glyphs")] = glyphUrl;
        qDebug() << "MapService: glyphs ->" << glyphUrl;
    }

    // Strip traffic overlay if disabled
    if (!m_settings->mapTrafficOverlay())
        removeTrafficFromStyle(root);

    // Flat footprints in 2D
    if (m_view2D)
        flattenBuildingExtrusions(root);

    injectRouteLayers(root);

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

QJsonObject MapService::flattenExtrusionLayer(QJsonObject layer)
{
    if (layer.value(QStringLiteral("type")).toString() != QLatin1String("fill-extrusion"))
        return layer;

    // Keep the id: theme overrides and the route layers' insert_before anchor
    // both resolve by it.
    const QJsonObject src = layer.value(QStringLiteral("paint")).toObject();
    QJsonObject flat;
    if (src.contains(QStringLiteral("fill-extrusion-color")))
        flat[QStringLiteral("fill-color")] = src.value(QStringLiteral("fill-extrusion-color"));
    if (src.contains(QStringLiteral("fill-extrusion-opacity")))
        flat[QStringLiteral("fill-opacity")] = src.value(QStringLiteral("fill-extrusion-opacity"));

    layer[QStringLiteral("type")] = QStringLiteral("fill");
    layer[QStringLiteral("paint")] = flat;
    return layer;
}

void MapService::flattenBuildingExtrusions(QJsonObject &root)
{
    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    QJsonArray out;
    for (const QJsonValue &v : layers)
        out.append(flattenExtrusionLayer(v.toObject()));
    root[QStringLiteral("layers")] = out;
}

void MapService::updateTiltForZoom()
{
    // Tilt rides the same smoothed zoom the camera already uses, so the pair
    // reads as one move instead of two independently timed ones.
    const double span = MaxZoom - MinZoom;
    const double p = std::clamp((m_currentZoom - MinZoom) / span, 0.0, 1.0);
    const double tilt = MapTiltFar + (MapTiltNear - MapTiltFar) * p;
    if (std::abs(tilt - m_mapTilt) > 0.01) {
        m_mapTilt = tilt;
        emit mapTiltChanged();
    }
}

void MapService::debugZoomBy(double delta)
{
    if (!m_debugZoomEnabled)
        return;

    m_debugZoomActive = true;
    // Deliberately wider than MinZoom/MaxZoom: the point is to inspect what the
    // tiles hold above and below the range the dashboard normally shows.
    m_currentZoom = std::clamp(m_currentZoom + delta, DebugMinZoom, DebugMaxZoom);
    if (m_currentZoom != m_mapZoom && std::isfinite(m_currentZoom)) {
        m_mapZoom = m_currentZoom;
        emit mapZoomChanged();
    }
    updateTiltForZoom();
}

void MapService::debugResetZoom()
{
    if (!m_debugZoomEnabled)
        return;

    m_debugZoomActive = false;
    m_targetZoom = DefaultZoom;
}


void MapService::injectRouteLayers(QJsonObject &root)
{
    // The route has to sit at a specific depth: under the building extrusions
    // so they occlude it, with a translucent copy above them so it stays
    // followable through a block, and under the street labels so names are not
    // painted over. QMapLibre's LayerParameter cannot express an insertion
    // point, so the layers are placed here instead of being added from QML.
    QJsonObject sources = root.value(QStringLiteral("sources")).toObject();
    QJsonObject empty;
    empty[QStringLiteral("type")] = QStringLiteral("FeatureCollection");
    empty[QStringLiteral("features")] = QJsonArray();
    QJsonObject routeSource;
    routeSource[QStringLiteral("type")] = QStringLiteral("geojson");
    routeSource[QStringLiteral("data")] = empty;
    sources[QStringLiteral("route")] = routeSource;
    root[QStringLiteral("sources")] = sources;

    auto line = [](const QString &id, const QString &color, double width, double opacity) {
        QJsonObject layout;
        layout[QStringLiteral("line-cap")] = QStringLiteral("round");
        layout[QStringLiteral("line-join")] = QStringLiteral("round");
        QJsonObject paint;
        paint[QStringLiteral("line-color")] = color;
        paint[QStringLiteral("line-width")] = width;
        if (opacity < 1.0)
            paint[QStringLiteral("line-opacity")] = opacity;
        QJsonObject layer;
        layer[QStringLiteral("id")] = id;
        layer[QStringLiteral("type")] = QStringLiteral("line");
        layer[QStringLiteral("source")] = QStringLiteral("route");
        layer[QStringLiteral("layout")] = layout;
        layer[QStringLiteral("paint")] = paint;
        return layer;
    };

    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    int firstExtrusion = -1;
    int lastExtrusion = -1;
    int firstSymbol = -1;
    for (int i = 0; i < layers.size(); ++i) {
        const QString type = layers.at(i).toObject().value(QStringLiteral("type")).toString();
        if (type == QLatin1String("fill-extrusion")) {
            if (firstExtrusion < 0)
                firstExtrusion = i;
            lastExtrusion = i;
        } else if (type == QLatin1String("symbol") && firstSymbol < 0) {
            firstSymbol = i;
        }
    }

    QJsonArray out;
    for (int i = 0; i < layers.size(); ++i) {
        // With no extrusions (2D) there is nothing to hide behind, so the
        // solid route goes straight under the labels and no ghost is drawn.
        if (i == firstExtrusion || (firstExtrusion < 0 && i == firstSymbol)) {
            out.append(line(QStringLiteral("route-border"), QStringLiteral("#1565C0"), 11, 1.0));
            out.append(line(QStringLiteral("route-fill"), QStringLiteral("#42A5F5"), 7, 1.0));
        }
        out.append(layers.at(i));
        if (firstExtrusion >= 0 && i == lastExtrusion) {
            out.append(line(QStringLiteral("route-ghost"), QStringLiteral("#42A5F5"), 7, 0.35));
        }
    }
    if (firstExtrusion < 0 && firstSymbol < 0) {
        out.append(line(QStringLiteral("route-border"), QStringLiteral("#1565C0"), 11, 1.0));
        out.append(line(QStringLiteral("route-fill"), QStringLiteral("#42A5F5"), 7, 1.0));
    }
    root[QStringLiteral("layers")] = out;
}

QString MapService::localGlyphDirectory() const
{
    QStringList candidates;
    const QString envDir = qEnvironmentVariable("SCOOTUI_GLYPH_DIR");
    if (!envDir.isEmpty())
        candidates << envDir;
    candidates << QStringLiteral("/usr/share/scootui/glyphs");

    for (const QString &path : std::as_const(candidates)) {
        QDir dir(path);
        if (!dir.exists())
            continue;
        // Require a fontstack that actually carries the Latin range; a directory
        // that exists but is empty would stall tiles exactly like a 404.
        const QStringList stacks = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &stack : stacks) {
            if (QFile::exists(dir.filePath(stack + QStringLiteral("/0-255.pbf"))))
                return path;
        }
        qWarning() << "MapService: glyph directory" << path << "has no usable fontstack";
    }
    return QString();
}

QString MapService::styleVariantSuffix() const
{
    QString s;
    if (!m_settings->mapTrafficOverlay())
        s += QStringLiteral("-notraffic");
    if (m_view2D)
        s += QStringLiteral("-2d");
    return s;
}

QString MapService::rewriteStyleVariant(const QString &qrcPath)
{
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);
    QString stem = baseName.chopped(5);  // strip ".json"
    QString outPath = QStringLiteral("/tmp/") + stem + styleVariantSuffix()
                      + QStringLiteral(".json");

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
    if (!m_settings->mapTrafficOverlay())
        removeTrafficFromStyle(root);
    if (m_view2D)
        flattenBuildingExtrusions(root);
    injectRouteLayers(root);

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

// The ECU owns speed whenever it is talking, including when it reports 0: that
// is a parked scooter, not a missing reading. Only once ecu-service raises E20
// does a 0 mean "no idea" - that is the state the cluster renders as "-"
// instead of a number - and GPS stands in. A stationary receiver still reports
// a few km/h of noise, so GPS speed only counts above GpsSpeedTrustKmh; below
// that we would rather have no motion signal than a fictitious one.
double MapService::effectiveSpeedKmh() const
{
    if (m_engine->faultCode() != EcuCommLostFaultCode)
        return m_engine->speed();

    const double gpsKmh = m_gps ? m_gps->speed() : 0.0;
    return gpsKmh >= GpsSpeedTrustKmh ? gpsKmh : 0.0;
}

// ---------------------------------------------------------------------------
// Dead reckoning tick (20 Hz)
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

    // ----- Physical estimate -----
    double speedKmh = effectiveSpeedKmh();
    double speedMs = speedKmh * (1000.0 / 3600.0);
    bool stationary = speedMs < StationarySpeedMs;
    m_positionUncertaintyMeters = std::min(
        MaxPositionUncertaintyMeters,
        m_positionUncertaintyMeters + dt * (0.5 + speedMs * 0.03));
    const double distMeters = m_odometerReconciler.advance(
        m_engine->odometer(), stationary ? 0.0 : speedMs, dt);

    const bool haveRouteShape = m_routeShape.size() >= 2
        && m_currentRouteSegment >= 0;
    // Observe deviation transitions on the estimator clock, not only on GPS
    // arrivals. Rejoining during a GPS gap must still release the segment
    // high-water mark so an overshoot-and-reverse can match an earlier leg.
    const bool nowOffRoute = m_navigation && m_navigation->isOffRoute();
    const bool justRejoinedRoute = m_lastWasOffRoute && !nowOffRoute;
    if (justRejoinedRoute)
        m_maxReachedSegment = -1;
    m_lastWasOffRoute = nowOffRoute;
    bool routeUsable = RoutePresentationPolicy::shouldSnapToRoute(
        haveRouteShape, m_navigation->isOffRoute(),
        m_navigation->isRerouting());

    // Route geometry may inform the motion direction while navigation still
    // trusts the route, but it never overwrites the independent coordinate.
    // Strong fresh direction disagreement steers that physical estimate before
    // the 60 m off-route threshold, improving a later reroute origin without
    // changing presentation or initiating an early reroute.
    const GpsSample sample = m_gps->currentSample();
    double physicalBearing = m_displayBearing;
    if (routeUsable) {
        const double rb = routeSegmentBearing();
        if (rb >= 0.0) {
            physicalBearing = rb;
            const bool courseReliable = m_gps->hasRecentFix()
                && m_gps->timestampAgeMs() <= SnapCourseMaxAgeMs
                && sample.speedKmh >= SnapCourseMinSpeedKmh
                && std::isfinite(sample.course);
            if (RoutePresentationPolicy::hasDirectionalDepartureEvidence(
                    m_distFromRoute, signedAngleDiff(sample.course, rb),
                    courseReliable)) {
                physicalBearing = sample.course;
            }
        }
    }
    projectPositionStraight(distMeters, physicalBearing);

    if (m_gps->hasRecentFix()
        && sample.hasAcceptableAccuracy(MaxEstimatorEphMeters)) {
        blendGpsCorrection(dt, stationary ? StationaryGpsBlendScale : 1.0);
    }

    // Advance the segment identity from the physical estimate between 1 Hz
    // GPS samples. The route bearing is only a trajectory hint here; distance
    // and hysteresis still decide the match.
    if (!stationary && routeUsable) {
        const bool gpsTrajectory = m_gps->hasRecentFix()
            && sample.speedKmh >= MinSpeedForTrajectoryKmh;
        updateRouteMatch(m_drLatitude, m_drLongitude,
                         gpsTrajectory ? sample.course : physicalBearing,
                         gpsTrajectory || speedKmh >= MinSpeedForTrajectoryKmh);
    }

    if (haveRouteShape) {
        // Segment projection is cheap and stays at render rate. The global
        // nearest scan used for off-route distance runs at the same exact 5 Hz
        // phase as NavigationService instead of scanning the full route 20 Hz.
        const bool fullProjection = m_projectionCadence.advance();
        refreshRouteProjection(fullProjection);
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
                          && !m_navigation->isOffRoute()
                          && !m_navigation->isRerouting();
    if (onRouteForComp) {
        double rb = presentationRouteBearing();
        if (rb >= 0) compensationBearing = rb;
    }
    // Presentation may be route matched; the physical pose above remains
    // untouched and continues to drive off-route/reroute decisions.
    double presentationLat = m_drLatitude;
    double presentationLng = m_drLongitude;
    if (routeUsable && m_currentRouteSegment >= 0) {
        m_freeDriveSnapState.reset();
        presentationLat = m_segmentSnappedLat;
        presentationLng = m_segmentSnappedLng;
    } else if (m_routeShape.isEmpty() && m_roadInfo) {
        // In free-drive mode, use the same heading-aware road segment that
        // supplies street/speed metadata. Project the continuously advancing
        // physical estimate on every render tick, so the marker moves smoothly
        // along the road while the heavier tile match runs only once per second.
        const bool confident = m_roadInfo->hasConfidentRoadMatch();
        double roadLat = m_drLatitude;
        double roadLng = m_drLongitude;
        double roadDistance = std::numeric_limits<double>::infinity();
        if (confident) {
            projectOntoSegment(
                m_drLatitude, m_drLongitude,
                m_roadInfo->matchedSegmentLat1(), m_roadInfo->matchedSegmentLon1(),
                m_roadInfo->matchedSegmentLat2(), m_roadInfo->matchedSegmentLon2(),
                roadLat, roadLng, roadDistance);
        }
        if (m_freeDriveSnapState.update(confident, roadDistance)) {
            presentationLat = roadLat;
            presentationLng = roadLng;
        }
    } else {
        m_freeDriveSnapState.reset();
    }

    double compensatedLat = presentationLat;
    double compensatedLng = presentationLng;
    if (speedMs > 0.5) {
        projectForward(presentationLat, presentationLng, compensationBearing,
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
    // updated DR position. Navigation and road-info consumers divide this
    // shared 20 Hz cadence by exact integer ratios.
    emit vehiclePositionChanged();

    // ----- isReady -----
    if (!m_isReady && m_hasInitialPosition) {
        m_isReady = true;
        emit isReadyChanged();
    }
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

void MapService::blendGpsCorrection(double dt, double rateScale)
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

    double factor = std::min(1.0, blendRate * rateScale * dt);

    m_drLatitude += m_gpsErrorLatitude * factor;
    m_drLongitude += m_gpsErrorLongitude * factor;

    m_gpsErrorLatitude *= (1.0 - factor);
    m_gpsErrorLongitude *= (1.0 - factor);
}

// ---------------------------------------------------------------------------
// Dynamic zoom
// ---------------------------------------------------------------------------

void MapService::updateDynamicZoom(double dt)
{
    // Wheel zoom has taken over; leave the camera where the developer put it.
    if (m_debugZoomActive)
        return;

    double effectiveTarget;
    double smoothRate;
    double minClamp;

    if (m_routeOverviewActive) {
        // During overview: zoom out to the extent-derived target faster.
        effectiveTarget = m_overviewZoom;
        smoothRate = OverviewZoomRate;
        minClamp = OverviewMinZoom;
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

    updateTiltForZoom();
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

        // A close second maneuver needs a wider view that contains both turns.
        double dist2 = distanceToSecondManeuver();
        dist = MapCameraPolicy::maneuverFocusDistance(
            dist, dist2, MultiTurnLookAheadMeters);
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

double MapService::presentationRouteBearing() const
{
    if (m_routeShape.size() < 2 || m_presentationRouteSegment < 0
        || m_presentationRouteSegment >= m_routeShape.size() - 1)
        return routeSegmentBearing();
    const int seg = m_presentationRouteSegment;
    return bearingBetween(m_routeShape[seg].first, m_routeShape[seg].second,
                          m_routeShape[seg + 1].first, m_routeShape[seg + 1].second);
}

void MapService::updateBearing(double dt)
{
    double speedKmh = effectiveSpeedKmh();

    // Presentation remains on the route until NavigationService's authoritative
    // off-route policy fires; then marker position and rotation switch to the
    // physical estimate together. While snapped, use the segment the presented
    // point has actually walked onto rather than the matcher's lagging identity
    // segment; otherwise the marker rounds a corner sideways.
    bool onRoute = !m_routeShape.isEmpty() && m_currentRouteSegment >= 0
                   && m_navigation->isNavigating()
                   && !m_navigation->isOffRoute()
                   && !m_navigation->isRerouting();
    bool hasFix = m_gps->hasRecentFix();
    double gpsCourse = m_gps->course();

    // Fresh, reasonably-accurate magnetic heading available? motion-service
    // floors accuracy at 2.5deg and inflates it with tilt/accel/yaw, so a
    // small value means a trustworthy reading. accuracyDeg is 0 only before
    // the first push, which the freshness gate also rejects.
    const bool magOk = m_motion
        && m_headingAge.isValid()
        && m_headingAge.elapsed() <= MagHeadingMaxAgeMs
        && m_motion->accuracyDeg() > 0.0
        && m_motion->accuracyDeg() <= MagHeadingMaxAccuracyDeg;
    const double magHeading = magOk
        ? std::fmod(m_motion->headingDeg() + 360.0, 360.0)
        : -1.0;

    // GPS course only means something with a fix and above the freeze speed.
    const bool gpsCourseUsable = hasFix && speedKmh >= HeadingFreezeSpeed;

    // Off-route with no usable GPS course: steer by the magnetic compass if we
    // have one, otherwise hold the last smoothed bearing rather than snapping
    // to a stale gpsCourse.
    if (!onRoute && !gpsCourseUsable && !magOk)
        return;

    double rawHeading;
    if (onRoute && !hasFix) {
        // DR on route: use route bearing only
        double rb = presentationRouteBearing();
        rawHeading = (rb >= 0) ? rb : gpsCourse;
    } else if (onRoute && hasFix) {
        // On-route: route geometry is authoritative. gpsCourse lags through
        // turns (reported ≤1 Hz; between updates it still shows the pre-turn
        // direction), and any blend with the stale course dragged
        // m_smoothedTarget back toward the old bearing for a beat after a
        // turn-snap — the map would swing forward, bounce partway back, then
        // settle. If the rider genuinely deviates, off-route detection kicks
        // in and the else branch takes over with pure gpsCourse.
        double rb = presentationRouteBearing();
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

    // When GPS course is unusable (no fix, or below the freeze speed) and we
    // are not following a route, orient by the magnetic compass so the map
    // reflects the scooter's actual facing instead of a stale/frozen course.
    const bool steerByMag = (!onRoute && !gpsCourseUsable && magOk);
    if (steerByMag)
        rawHeading = magHeading;

    // Speed-based damping factor: freeze below HeadingFreezeSpeed, ramp to full
    double dampFactor = 0;
    if (speedKmh >= HeadingFullSpeed) {
        dampFactor = 1.0;
    } else if (speedKmh > HeadingFreezeSpeed) {
        dampFactor = (speedKmh - HeadingFreezeSpeed) / (HeadingFullSpeed - HeadingFreezeSpeed);
    }

    if (dampFactor < 0.001) {
        // Below the freeze speed the GPS course is useless. If we are steering
        // by the magnetic compass, keep updating with a gentle blend so the map
        // tracks the scooter's facing while stopped; otherwise hold the bearing.
        if (steerByMag)
            dampFactor = MagHeadingDamp;
        else
            return;
    }

    // Turn-snap: the route segment bearing jumped (segment boundary with a
    // real corner). Skip stage 1's speed-weighted exponential blend — just
    // jump the target to the new segment so stage 2 below can animate
    // m_displayBearing toward it at MaxBearingRate. ~0.8 s for a 90° turn,
    // still noticeably snappy but no longer a hard camera cut.
    double rbNow = onRoute ? presentationRouteBearing() : -1;
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

void MapService::updateRouteMatch(double lat, double lng,
                                  double trajectoryBearing,
                                  bool haveTrajectory)
{
    SegmentMatch match = matchRouteSegment(lat, lng, trajectoryBearing,
                                           haveTrajectory,
                                           m_currentRouteSegment);
    if (match.index < 0 || match.perpDist >= MatchAcceptanceDistance)
        return;

    bool accept = false;
    if (m_currentRouteSegment < 0
        || m_currentRouteSegment >= m_routeShape.size() - 1) {
        accept = true;
    } else if (match.index != m_currentRouteSegment) {
        const auto &a = m_routeShape[m_currentRouteSegment];
        const auto &b = m_routeShape[m_currentRouteSegment + 1];
        double ignoredLat, ignoredLng, currentDistance;
        projectOntoSegment(lat, lng, a.first, a.second, b.first, b.second,
                           ignoredLat, ignoredLng, currentDistance);
        double currentCost = currentDistance - CurrentSegmentBonus;
        if (haveTrajectory) {
            const double segmentBearing = bearingBetween(a.first, a.second,
                                                         b.first, b.second);
            const double difference = std::abs(
                signedAngleDiff(trajectoryBearing, segmentBearing));
            if (difference > 90.0) {
                currentCost += ReverseDirectionPenalty
                    + (difference - 90.0) * ReverseSlopePerDeg;
            } else {
                currentCost += difference * SoftDirectionFactor;
            }
        }
        accept = match.cost + SwitchHysteresis < currentCost;
    }

    if (accept) {
        m_currentRouteSegment = match.index;
        m_maxReachedSegment = std::max(m_maxReachedSegment,
                                       m_currentRouteSegment);
    }
}

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

void MapService::refreshRouteProjection(bool fullScan)
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
        m_presentationRouteSegment = -1;
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
    double sLat = m_snappedLat;
    double sLng = m_snappedLng;
    double dist = m_distFromRoute;
    if (fullScan) {
        sLat = m_drLatitude;
        sLng = m_drLongitude;
        dist = std::numeric_limits<double>::max();
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
    }

    // Segment-aligned projection: snap DR position onto the matcher's current
    // segment specifically (not the geometrically nearest). NavigationService
    // uses this for the along-route walker so that TBT distance-to-next-turn
    // stays consistent with the segment index even when the matcher and the
    // global-nearest disagree (HWM gate, direction penalty, post-reroute).
    if (m_currentRouteSegment >= 0 &&
        m_currentRouteSegment + 1 < m_routeShape.size()) {
        const auto &A = m_routeShape[m_currentRouteSegment];
        const auto &B = m_routeShape[m_currentRouteSegment + 1];
        m_presentationRouteSegment = m_currentRouteSegment;
        // The matcher is deliberately sticky: CurrentSegmentBonus plus
        // SwitchHysteresis keep m_currentRouteSegment on the leg it is on
        // until the next one wins by ~7.5 m of cost. That stickiness is right
        // for identity and wrong for geometry. Clamping the projection to the
        // current leg froze the presented position on the leg's far vertex for
        // the whole hysteresis window, then teleported it the accumulated
        // ~7.5-8 m the instant the matcher committed. Measured on a 25 km/h
        // sim run: ~12 ticks at exactly 0.000 m followed by one tick of
        // 7.5-8.3 m, once per route vertex.
        //
        // So carry the overshoot along the polyline instead of clamping it
        // away. Inside the leg this is the same projection as before; past
        // either end it walks the arc length the clamp used to throw away, so
        // the point keeps moving at the vehicle's own speed and the matcher's
        // later commit lands where the walk already is. Segment identity is
        // untouched.
        //
        // The leg stays the matcher's own. Picking the nearest leg from a
        // short window around it instead looks equivalent and is not: at a
        // 90-degree corner the estimate runs wide of the vertex, both legs are
        // then several metres away, and the nearest of the two flips back and
        // forth for the ~1 s it takes GPS to pull the estimate back onto the
        // route. Measured over five right-angle corners at 25 km/h, a +-1/+2
        // window turned 7 corner ticks above 1 m into 28, and added forward
        // spikes of up to 8 m to the backward ones; widening it to +-10 changed
        // nothing, because the flip is always between the two legs that meet at
        // the corner.
        double segDist = 0.0;
        projectOntoSegment(m_drLatitude, m_drLongitude,
                           A.first, A.second, B.first, B.second,
                           m_segmentSnappedLat, m_segmentSnappedLng, segDist);
        (void)segDist;
        double t = 0.0, segLength = 0.0;
        const bool moving = RoutePresentationPolicy::allowsPolylineWalk(
            effectiveSpeedKmh());
        if (moving
            && segmentParam(m_drLatitude, m_drLongitude,
                            A.first, A.second, B.first, B.second, t, segLength)
            && (t < 0.0 || t > 1.0)) {
            if (t > 1.0) {
                walkPolyline(m_routeShape, m_currentRouteSegment + 1,
                             (t - 1.0) * segLength,
                             m_segmentSnappedLat, m_segmentSnappedLng,
                             &m_presentationRouteSegment);
            } else {
                walkPolyline(m_routeShape, m_currentRouteSegment, t * segLength,
                             m_segmentSnappedLat, m_segmentSnappedLng,
                             &m_presentationRouteSegment);
            }
        }
    } else {
        m_segmentSnappedLat = m_drLatitude;
        m_segmentSnappedLng = m_drLongitude;
        m_presentationRouteSegment = -1;
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
