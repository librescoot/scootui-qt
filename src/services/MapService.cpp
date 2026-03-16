#include "MapService.h"
#include "NavigationService.h"
#include "stores/GpsStore.h"
#include "stores/EngineStore.h"
#include "stores/SettingsStore.h"
#include "stores/ThemeStore.h"
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

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MapService::MapService(GpsStore *gps, EngineStore *engine,
                       NavigationService *navigation, SettingsStore *settings,
                       ThemeStore *theme, QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_engine(engine)
    , m_navigation(navigation)
    , m_settings(settings)
    , m_theme(theme)
    , m_tickTimer(new QTimer(this))
{
    // Resolve mbtiles path: prefer ./map.mbtiles (desktop/simulator), fall back to device path
    if (QFile::exists(QStringLiteral("map.mbtiles"))) {
        m_mbtilesPath = QDir::currentPath() + QStringLiteral("/map.mbtiles");
        qDebug() << "MapService: using local mbtiles:" << m_mbtilesPath;
    } else if (QFile::exists(QStringLiteral("/data/maps/map.mbtiles"))) {
        m_mbtilesPath = QStringLiteral("/data/maps/map.mbtiles");
        qDebug() << "MapService: using device mbtiles:" << m_mbtilesPath;
    } else {
        qDebug() << "MapService: no mbtiles found, using online tiles";
    }

    // Build initial style URL
    rebuildStyleUrl();

    // Load mbtiles bounds for out-of-coverage detection
    loadMbtilesBounds();

    // --- GPS position updates ---
    connect(m_gps, &GpsStore::latitudeChanged, this, &MapService::onGpsPositionChanged);
    connect(m_gps, &GpsStore::longitudeChanged, this, &MapService::onGpsPositionChanged);

    // --- Route changes ---
    connect(m_navigation, &NavigationService::routeChanged, this, &MapService::onRouteChanged);

    // --- Theme changes ---
    connect(m_theme, &ThemeStore::themeChanged, this, &MapService::onThemeChanged);

    // --- Map type changes (online / offline) ---
    connect(m_settings, &SettingsStore::mapTypeChanged, this, &MapService::onMapTypeChanged);

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

    // Reset segment tracking
    m_currentRouteSegment = 0;

    updateRouteGeoJson();
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

    m_routeCoordinates.clear();
    emit routeCoordinatesChanged();

    updateRouteGeoJson();

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
        m_smoothedTarget = m_gps->course();
        m_displayBearing = m_gps->course();

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

    // Compute GPS error (distance from DR position to new GPS fix)
    double error = haversineDistance(m_drLatitude, m_drLongitude, gpsLat, gpsLng);

    if (error > SnapUpperThreshold) {
        // Extreme error: immediate reset
        m_drLatitude = gpsLat;
        m_drLongitude = gpsLng;
        m_gpsErrorLatitude = 0;
        m_gpsErrorLongitude = 0;
        m_isSnapping = false;
    } else if (error > SnapThreshold) {
        // Large jump: animated snap over SnapAnimationDuration
        m_isSnapping = true;
        m_snapProgress = 0;
        m_snapStartLat = m_drLatitude;
        m_snapStartLng = m_drLongitude;
        m_snapTargetLat = gpsLat;
        m_snapTargetLng = gpsLng;
    } else {
        // Normal / large error: accumulate correction for gradual blending
        m_gpsErrorLatitude = gpsLat - m_drLatitude;
        m_gpsErrorLongitude = gpsLng - m_drLongitude;
    }

    m_lastGpsLatitude = gpsLat;
    m_lastGpsLongitude = gpsLng;

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

void MapService::rebuildStyleUrl()
{
    bool isDark = m_theme->isDark();
    bool useLocal = !m_mbtilesPath.isEmpty();

    qDebug() << "MapService: rebuildStyleUrl - dark:" << isDark
             << "mbtiles:" << (useLocal ? m_mbtilesPath : QStringLiteral("none"));

    QString qrcPath = isDark
        ? QStringLiteral("qrc:/ScootUI/assets/styles/mapdark.json")
        : QStringLiteral("qrc:/ScootUI/assets/styles/maplight.json");

    QString url;
    if (useLocal) {
        url = rewriteStyleForMbtiles(qrcPath, m_mbtilesPath);
    } else {
        // Extract embedded style to temp file so MapLibre plugin can read it
        // (qrc:// paths aren't supported by the geoservices plugin)
        url = extractStyleToFile(qrcPath);
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
    // Determine output path
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);  // "mapdark.json" or "maplight.json"
    QString outPath = QStringLiteral("/tmp/") + baseName;

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

QString MapService::extractStyleToFile(const QString &qrcPath)
{
    QString baseName = qrcPath.section(QLatin1Char('/'), -1);
    QString outPath = QStringLiteral("/tmp/") + baseName;

    QString qrcFile = qrcPath;
    qrcFile.replace(QStringLiteral("qrc:/"), QStringLiteral(":/"));
    QFile f(qrcFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "MapService: cannot open embedded style" << qrcFile;
        return qrcPath;
    }

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MapService: cannot write" << outPath;
        f.close();
        return qrcPath;
    }
    out.write(f.readAll());
    f.close();
    out.close();

    QString fileUrl = QStringLiteral("file://") + outPath;
    qDebug() << "MapService: extracted style to" << fileUrl;
    return fileUrl;
}

// ---------------------------------------------------------------------------
// Dead reckoning tick (15 Hz)
// ---------------------------------------------------------------------------

void MapService::onDeadReckoningTick()
{
    if (!m_hasInitialPosition)
        return;

    double dtMs = static_cast<double>(m_elapsed.restart());
    double dt = dtMs / 1000.0;

    // Clamp to avoid huge jumps after app resume
    if (dt > 0.5)
        dt = 0.5;

    // ----- Speed (from ECU, km/h -> m/s) -----
    double speedKmh = m_engine->speed();
    double speedMs = speedKmh * (1000.0 / 3600.0) * SpeedFactor;

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
        double distMeters = speedMs * dt;

        if (!m_routeShape.isEmpty() && m_currentRouteSegment >= 0) {
            projectPositionAlongRoute(distMeters);
        } else {
            double heading = m_gps->course();
            projectPositionStraight(distMeters, heading);
        }

        // ----- GPS correction blending (only when GPS fix is recent) -----
        if (m_gps->hasRecentFix()) {
            blendGpsCorrection(dt);
        }
    }

    // ----- Latency compensation -----
    // Project the displayed position forward to compensate for GPS latency,
    // without modifying the internal DR state.
    double compensatedLat = m_drLatitude;
    double compensatedLng = m_drLongitude;
    if (speedMs > 0.5) {
        double heading = m_gps->course();
        projectForward(m_drLatitude, m_drLongitude, heading,
                       speedMs * LatencyCompensationSec,
                       compensatedLat, compensatedLng);
    }

    // ----- Update bearing & zoom first (needed for offset calculation) -----
    updateBearing(dt);
    updateDynamicZoom(dt);

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

        if (remaining >= distOnSeg && seg < m_routeShape.size() - 2) {
            // Advance to next segment
            m_drLatitude = nextLat;
            m_drLongitude = nextLng;
            remaining -= distOnSeg;
            seg++;
        } else {
            // Interpolate within current segment
            double brng = bearingBetween(m_drLatitude, m_drLongitude, nextLat, nextLng);
            double advance = std::min(remaining, distOnSeg);
            projectForward(m_drLatitude, m_drLongitude, brng, advance,
                           m_drLatitude, m_drLongitude);
            remaining = 0;
        }
    }

    m_currentRouteSegment = seg;
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
// Latency compensation (projects displayed position forward)
// ---------------------------------------------------------------------------

void MapService::applyLatencyCompensation(double /*speedMs*/, double /*headingDeg*/)
{
    // Actual projection done inline in onDeadReckoningTick for clarity.
    // This method is a placeholder for any additional latency logic.
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
    if (!m_navigation->hasRoute() || !m_navigation->isNavigating())
        return DefaultZoom;

    double dist = distanceToNextManeuver();
    if (dist <= 0)
        return DefaultZoom;

    // Multi-turn look-ahead: if a second maneuver is within 150m, use the closer one
    double dist2 = distanceToSecondManeuver();
    if (dist2 > 0 && dist2 < MultiTurnLookAheadMeters) {
        dist = std::min(dist, dist2);
    }

    // Logarithmic zoom formula:
    //   zoom = MaxZoom - (MaxZoom - MinZoom) * log2(dist / 50) / log2(2000 / 50)
    // Clamped to [MinZoom, MaxZoom]
    constexpr double NearDist = 50.0;
    constexpr double FarDist = 2000.0;
    constexpr double LogRange = 5.3219; // log2(2000/50) ~ log2(40)

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

void MapService::updateBearing(double dt)
{
    double speedKmh = m_engine->speed();
    double rawHeading = m_gps->course();

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

    // Stage 1: exponential blend toward target (matches Flutter)
    // Each frame moves a proportion of remaining distance, not a fixed step
    double targetDelta = normalizeAngle(rawHeading - m_smoothedTarget);
    double targetBlend = std::min(1.0, TargetSmoothRate * dt * dampFactor);
    m_smoothedTarget += targetDelta * targetBlend;
    m_smoothedTarget = std::fmod(m_smoothedTarget + 360.0, 360.0);

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
// Find closest route segment
// ---------------------------------------------------------------------------

int MapService::findClosestSegment(double lat, double lng) const
{
    if (m_routeShape.size() < 2)
        return -1;

    int bestIdx = 0;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = 0; i < m_routeShape.size() - 1; ++i) {
        // Simple: distance to segment start as heuristic
        double d = haversineDistance(lat, lng,
                                     m_routeShape[i].first, m_routeShape[i].second);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }

    return bestIdx;
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
