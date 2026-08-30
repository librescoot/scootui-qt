#include "RoadInfoService.h"
#include "AddressDatabaseService.h"
#include "MapService.h"
#include "NavigationService.h"
#include "RoadMatchPolicy.h"
#include "stores/GpsStore.h"
#include "stores/SpeedLimitStore.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QVariantMap>
#include <QtMath>
#include <algorithm>
#include <limits>

static const QSet<QString> s_roadTypes = {
    QStringLiteral("motorway"), QStringLiteral("trunk"),
    QStringLiteral("primary"), QStringLiteral("secondary"),
    QStringLiteral("tertiary"), QStringLiteral("unclassified"),
    QStringLiteral("residential"), QStringLiteral("living_street"),
    QStringLiteral("service")
};

namespace {
constexpr double EarthRadiusMeters = 6371000.0;

void projectOntoSegment(double lat, double lon,
                        double lat1, double lon1,
                        double lat2, double lon2,
                        double &snappedLat, double &snappedLon,
                        double &distance)
{
    const double cosLat = std::max(0.01, std::cos(lat * M_PI / 180.0));
    const double ax = (lon1 - lon) * M_PI / 180.0
        * EarthRadiusMeters * cosLat;
    const double ay = (lat1 - lat) * M_PI / 180.0 * EarthRadiusMeters;
    const double bx = (lon2 - lon) * M_PI / 180.0
        * EarthRadiusMeters * cosLat;
    const double by = (lat2 - lat) * M_PI / 180.0 * EarthRadiusMeters;
    const double dx = bx - ax;
    const double dy = by - ay;
    const double denominator = dx * dx + dy * dy;
    const double t = denominator > 1e-9
        ? std::clamp(-(ax * dx + ay * dy) / denominator, 0.0, 1.0)
        : 0.0;
    const double px = ax + t * dx;
    const double py = ay + t * dy;
    distance = std::hypot(px, py);
    snappedLat = lat + py / EarthRadiusMeters * 180.0 / M_PI;
    snappedLon = lon + px / (EarthRadiusMeters * cosLat) * 180.0 / M_PI;
}

double segmentBearing(double lat1, double lon1, double lat2, double lon2)
{
    const double dLon = (lon2 - lon1) * M_PI / 180.0;
    const double y = std::sin(dLon) * std::cos(lat2 * M_PI / 180.0);
    const double x = std::cos(lat1 * M_PI / 180.0)
            * std::sin(lat2 * M_PI / 180.0)
        - std::sin(lat1 * M_PI / 180.0)
            * std::cos(lat2 * M_PI / 180.0) * std::cos(dLon);
    if (std::abs(x) < 1e-10 && std::abs(y) < 1e-10)
        return -1.0;
    return std::fmod(std::atan2(y, x) * 180.0 / M_PI + 360.0, 360.0);
}
}

RoadInfoService::RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                                   NavigationService *navigation,
                                   QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_speedLimit(speedLimit)
    , m_navigation(navigation)
    , m_dbConnectionName(QStringLiteral("roadinfo_tiles"))
{
    m_lastUpdate.start();

    // Prefer local map.mbtiles (desktop/simulator), fall back to device path
    QString path = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : AddressDatabaseService::MbtilesPath;
    if (QFile::exists(path)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                      m_dbConnectionName);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            m_dbOpen = true;
            m_dbPath = path;
            m_dbMtime = QFileInfo(path).lastModified();
            qDebug() << "RoadInfoService: mbtiles database opened";
        } else {
            qWarning() << "RoadInfoService: failed to open mbtiles";
        }
    }

    // Connect the GPS signals unconditionally, even if the mbtiles isn't open
    // yet. On a cold boot scootui can start before /data is mounted, so the
    // file is absent here — but the route-driven road-name/speed path in
    // updateRoadInfo() needs no tile DB, and onGpsChanged() self-heals the DB
    // open once the file appears. Gating the connects on m_dbOpen used to leave
    // the whole pill dead (no writer) for the entire session in that race.
    connect(gps, &GpsStore::sampleChanged, this, &RoadInfoService::onGpsChanged);
}

RoadInfoService::~RoadInfoService()
{
    if (m_dbOpen) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_dbConnectionName);
    }
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
    QString path = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : AddressDatabaseService::MbtilesPath;

    if (!QFile::exists(path))
        return;

    // Idempotent: if the *same file* is already open, don't tear down the
    // SQLite connection and dump the tile LRU cache. This lets us call
    // reloadMbtiles() freely from the availability poller / file watcher without
    // churning the cache on every routing flap or redundant recovery trigger.
    // We key on path AND mtime: an OTA map install replaces map.mbtiles at the
    // same path with a new inode, so a path-only check would keep serving from
    // the stale (unlinked) fd and pin its disk space until restart.
    const QDateTime mtime = QFileInfo(path).lastModified();
    if (m_dbOpen && path == m_dbPath && mtime == m_dbMtime)
        return;

    // Close existing connection if open
    if (m_dbOpen) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_dbConnectionName);
        m_dbOpen = false;
        m_dbPath.clear();
        m_dbMtime = {};
        m_tileCache.clear();
        m_cacheOrder.clear();
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  m_dbConnectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (db.open()) {
        m_dbOpen = true;
        m_dbPath = path;
        m_dbMtime = mtime;
        qDebug() << "RoadInfoService: mbtiles database opened";
    } else {
        qWarning() << "RoadInfoService: failed to open mbtiles";
    }
}

void RoadInfoService::countMissAndMaybeClear()
{
    // The tile matcher runs at 1 Hz. Keep the previous confident segment over
    // two isolated misses so a tile boundary, momentary ambiguity at a crossing,
    // or one noisy fix cannot pop the marker off the road. MapService still
    // releases immediately if the physical estimate moves materially away from
    // that retained segment.
    if (!m_matchRetention.retainAfterMiss()) {
        clearRoadMatch();
        m_speedLimit->setRoadNameDirect(QString());
        m_speedLimit->setRoadRefsDirect(QString());
        m_speedLimit->setRoadTypeDirect(QString());
        m_speedLimit->setSpeedLimitDirect(QString());
        m_speedLimit->setRoadBearingDirect(-1);
    }
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
        m_matchRetention.reset();
        clearRoadMatch();
        m_speedLimit->setRoadNameDirect(QString());
        m_speedLimit->setRoadRefsDirect(QString());
        m_speedLimit->setRoadTypeDirect(QString());
        m_speedLimit->setSpeedLimitDirect(QString());
        m_speedLimit->setRoadBearingDirect(-1);
        return;
    }

    if (m_lastUpdate.elapsed() < FallbackUpdateIntervalMs)
        return;

    m_lastUpdate.restart();

    // Self-heal: if the mbtiles wasn't available when we constructed (cold boot
    // before /data mounted), pick it up as soon as it appears. Throttled to the
    // 1 Hz fallback above, and reloadMbtiles() is idempotent once open.
    if (!m_dbOpen)
        reloadMbtiles();

    updateRoadInfo(m_gps->latitude(), m_gps->longitude());
}

void RoadInfoService::onVehiclePositionChanged()
{
    if (!m_map || !m_map->hasVehiclePosition())
        return;
    if (!m_updateCadence.advance())
        return;
    if (!m_dbOpen)
        reloadMbtiles();
    updateRoadInfo(m_map->vehicleLatitude(), m_map->vehicleLongitude());
}

int RoadInfoService::lonToTileX(double lon, int zoom)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

int RoadInfoService::latToTileY(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    double n = std::pow(2.0, zoom);
    int slippyY = static_cast<int>(std::floor(
        (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n));
    // MBTiles uses TMS (Y=0 at bottom), convert from slippy (Y=0 at top)
    return static_cast<int>(n) - 1 - slippyY;
}

void RoadInfoService::updateRoadInfo(double lat, double lon)
{
    const bool hasRouteAttrs = m_navigation
        && m_navigation->hasCurrentEdgeAttrs();
    auto publishRouteAttrs = [this]() {
        m_speedLimit->setRoadNameDirect(m_navigation->currentEdgeName());
        m_speedLimit->setRoadRefsDirect(
            m_navigation->currentEdgeRefs().join(QStringLiteral(", ")));
        m_speedLimit->setRoadTypeDirect(m_navigation->currentEdgeRoadClass());
        const int kph = m_navigation->currentEdgeSpeedLimitKph();
        m_speedLimit->setSpeedLimitDirect(
            kph > 0 ? QString::number(kph) : QString());
    };

    // A complete route edge is authoritative and avoids doing any tile work.
    // Partial trace attributes fall through so missing name/class/speed fields
    // can be filled from the local vector tile.
    if (hasRouteAttrs
        && !m_navigation->currentEdgeName().isEmpty()
        && !m_navigation->currentEdgeRoadClass().isEmpty()
        && m_navigation->currentEdgeSpeedLimitKph() > 0) {
        publishRouteAttrs();
        clearRoadMatch();
        m_matchRetention.reset();
        return;
    }

    if (!m_dbOpen) {
        if (hasRouteAttrs) {
            publishRouteAttrs();
            clearRoadMatch();
        }
        return;
    }

    struct Candidate {
        RoadMatchCandidateScore policy;
        QString name;
        QString refs;
        QString kind;
        QString maxspeed;
        double lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
        double snappedLat = 0, snappedLon = 0;
        double actualDistanceMeters = std::numeric_limits<double>::max();
    };
    QList<Candidate> candidates;

    const int centerX = lonToTileX(lon, QueryZoom);
    const int centerY = latToTileY(lat, QueryZoom);
    const int tileCount = 1 << QueryZoom;
    const double n = std::pow(2.0, QueryZoom);

    // Search the 3x3 neighborhood. Looking only in the coordinate's own tile
    // misses roads just across a z14 boundary even when they are a few metres
    // away, producing a regular metadata blank near tile seams.
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oy = -1; oy <= 1; ++oy) {
            const int tileX = centerX + ox;
            const int tileY = centerY + oy;
            if (tileX < 0 || tileY < 0
                || tileX >= tileCount || tileY >= tileCount)
                continue;

            const quint64 cacheKey = (static_cast<quint64>(tileX) << 32)
                | static_cast<quint64>(static_cast<uint32_t>(tileY));
            if (!m_tileCache.contains(cacheKey)) {
                QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
                QSqlQuery query(db);
                query.prepare(QStringLiteral(
                    "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
                query.addBindValue(QueryZoom);
                query.addBindValue(tileX);
                query.addBindValue(tileY);
                if (!query.exec() || !query.next())
                    continue;
                const QByteArray decompressed =
                    VectorTile::gunzip(query.value(0).toByteArray());
                if (decompressed.isEmpty())
                    continue;
                while (m_cacheOrder.size() >= MaxCachedTiles) {
                    const quint64 evict = m_cacheOrder.takeFirst();
                    m_tileCache.remove(evict);
                }
                m_tileCache.insert(cacheKey, VectorTile::parse(decompressed));
            }
            m_cacheOrder.removeOne(cacheKey);
            m_cacheOrder.append(cacheKey);

            const VectorTile::Tile &tile = m_tileCache[cacheKey];
            const VectorTile::Layer *streetsLayer = nullptr;
            for (const auto &layer : tile.layers) {
                if (layer.name == QLatin1String("streets")) {
                    streetsLayer = &layer;
                    break;
                }
            }
            if (!streetsLayer)
                continue;
            const double extent = streetsLayer->extent;

            for (const auto &feature : streetsLayer->features) {
                if (feature.type != 2)
                    continue;
                const QString kind =
                    feature.properties.value(QStringLiteral("kind"));
                if (!s_roadTypes.contains(kind))
                    continue;
                const QVector<QVector<QPointF>> parts =
                    VectorTile::decodeLineStringParts(feature.geometry);
                if (parts.isEmpty())
                    continue;

                Candidate candidate;
                candidate.name =
                    feature.properties.value(QStringLiteral("name"));
                candidate.kind = kind;
                candidate.maxspeed =
                    feature.properties.value(QStringLiteral("maxspeed"));
                const QString refRaw =
                    feature.properties.value(QStringLiteral("ref"));
                QStringList refParts = refRaw.split(
                    QLatin1Char(';'), Qt::SkipEmptyParts);
                for (QString &part : refParts)
                    part = part.trimmed();
                candidate.refs = refParts.join(QStringLiteral(", "));
                candidate.policy.tunnel =
                    feature.properties.value(QStringLiteral("tunnel"))
                    == QLatin1String("true");
                candidate.policy.oneWay =
                    feature.properties.value(QStringLiteral("oneway"))
                    == QLatin1String("true");
                candidate.policy.oneWayReverse =
                    feature.properties.value(QStringLiteral("oneway_reverse"))
                    == QLatin1String("true");

                for (const QVector<QPointF> &points : parts) {
                    for (int i = 0; i + 1 < points.size(); ++i) {
                        const double lon1 = (tileX + points[i].x() / extent)
                            / n * 360.0 - 180.0;
                        const double mercY1 = 1.0 - (tileY + 1.0
                            - points[i].y() / extent) / n;
                        const double lat1 = std::atan(std::sinh(
                            M_PI * (1.0 - 2.0 * mercY1))) * 180.0 / M_PI;
                        const double lon2 = (tileX + points[i + 1].x() / extent)
                            / n * 360.0 - 180.0;
                        const double mercY2 = 1.0 - (tileY + 1.0
                            - points[i + 1].y() / extent) / n;
                        const double lat2 = std::atan(std::sinh(
                            M_PI * (1.0 - 2.0 * mercY2))) * 180.0 / M_PI;

                        double snappedLat, snappedLon, distance;
                        projectOntoSegment(lat, lon, lat1, lon1, lat2, lon2,
                                           snappedLat, snappedLon, distance);
                        if (distance < candidate.actualDistanceMeters) {
                            candidate.actualDistanceMeters = distance;
                            candidate.lat1 = lat1; candidate.lon1 = lon1;
                            candidate.lat2 = lat2; candidate.lon2 = lon2;
                            candidate.snappedLat = snappedLat;
                            candidate.snappedLon = snappedLon;
                        }
                    }
                }
                if (!std::isfinite(candidate.actualDistanceMeters))
                    continue;
                candidate.policy.distanceMeters =
                    candidate.actualDistanceMeters;
                candidate.policy.bearingDegrees = segmentBearing(
                    candidate.lat1, candidate.lon1,
                    candidate.lat2, candidate.lon2);
                candidate.policy.key = candidate.name
                    + QLatin1Char('|') + candidate.refs
                    + QLatin1Char('|') + candidate.kind
                    + QLatin1Char('|')
                    + QString::number((candidate.lat1 + candidate.lat2) * 0.5,
                                      'f', 5)
                    + QLatin1Char('|')
                    + QString::number((candidate.lon1 + candidate.lon2) * 0.5,
                                      'f', 5);
                candidates.append(candidate);
            }
        }
    }

    const GpsSample gps = m_gps ? m_gps->currentSample() : GpsSample{};
    const bool headingReliable = m_gps && gps.hasValidCoordinate()
        && gps.hasFix() && m_gps->timestampAgeMs() <= 2500
        && gps.speedKmh >= 3.0 && std::isfinite(gps.course);
    double maxDistance = 35.0;
    if (m_map)
        maxDistance = std::clamp(m_map->positionUncertaintyMeters() + 8.0,
                                 15.0, 40.0);
    else if (gps.ephMeters > 0.0)
        maxDistance = std::clamp(gps.ephMeters * 1.5 + 8.0, 15.0, 40.0);

    QList<RoadMatchCandidateScore> policyCandidates;
    policyCandidates.reserve(candidates.size());
    const QString routeName = m_navigation
        ? m_navigation->currentSegmentStreetName() : QString();
    for (const Candidate &candidate : candidates) {
        auto score = candidate.policy;
        if (!routeName.isEmpty()
            && candidate.name.compare(routeName, Qt::CaseInsensitive) == 0) {
            score.distanceMeters = std::max(0.0, score.distanceMeters - 12.0);
        }
        policyCandidates.append(score);
    }
    const RoadMatchSelection selection = RoadMatchPolicy::select(
        policyCandidates, gps.course, headingReliable,
        m_previousMatchKey, maxDistance);
    if (selection.index < 0) {
        if (hasRouteAttrs) {
            publishRouteAttrs();
            clearRoadMatch();
            m_matchRetention.reset();
        } else {
            countMissAndMaybeClear();
        }
        return;
    }

    const Candidate &chosen = candidates[selection.index];
    const bool freeDrive = !m_navigation || !m_navigation->hasRoute();
    if (freeDrive && !selection.confident) {
        // A ranked winner is not necessarily a trustworthy road at a crossing
        // or between close parallel carriageways. Retain the previous confident
        // match briefly instead of switching the marker to an ambiguous result.
        countMissAndMaybeClear();
        return;
    }

    m_matchRetention.matched();
    QString name = chosen.name;
    QString refs = chosen.refs;
    QString kind = chosen.kind;
    QString maxspeed = chosen.maxspeed;
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
    m_speedLimit->setRoadBearingDirect(chosen.policy.bearingDegrees);

    const bool confident = freeDrive && selection.confident;
    const bool changed = confident != m_hasConfidentRoadMatch
        || chosen.lat1 != m_matchLat1 || chosen.lon1 != m_matchLon1
        || chosen.lat2 != m_matchLat2 || chosen.lon2 != m_matchLon2;
    m_hasConfidentRoadMatch = confident;
    m_matchLat1 = chosen.lat1; m_matchLon1 = chosen.lon1;
    m_matchLat2 = chosen.lat2; m_matchLon2 = chosen.lon2;
    m_matchDistanceMeters = chosen.actualDistanceMeters;
    m_previousMatchKey = confident ? chosen.policy.key : QString();
    if (changed)
        emit roadMatchChanged();
}

QString RoadInfoService::lookupNearestAddress(double lat, double lon)
{
    if (!m_dbOpen)
        return {};

    int tileX = lonToTileX(lon, QueryZoom);
    int tileY = latToTileY(lat, QueryZoom);
    quint64 cacheKey = (static_cast<quint64>(tileX) << 32)
                       | static_cast<quint64>(static_cast<uint32_t>(tileY));

    // Get or load tile
    VectorTile::Tile *tile = nullptr;
    if (m_tileCache.contains(cacheKey)) {
        tile = &m_tileCache[cacheKey];
        m_cacheOrder.removeOne(cacheKey);
        m_cacheOrder.append(cacheKey);
    } else {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
        query.addBindValue(QueryZoom);
        query.addBindValue(tileX);
        query.addBindValue(tileY);

        if (!query.exec() || !query.next())
            return {};

        QByteArray tileData = query.value(0).toByteArray();
        QByteArray decompressed = VectorTile::gunzip(tileData);
        if (decompressed.isEmpty())
            return {};

        while (m_cacheOrder.size() >= MaxCachedTiles) {
            quint64 evict = m_cacheOrder.takeFirst();
            m_tileCache.remove(evict);
        }

        m_tileCache.insert(cacheKey, VectorTile::parse(decompressed));
        m_cacheOrder.append(cacheKey);
        tile = &m_tileCache[cacheKey];
    }

    // Find addresses layer
    const VectorTile::Layer *addrLayer = nullptr;
    for (const auto &layer : tile->layers) {
        if (layer.name == QLatin1String("addresses")) {
            addrLayer = &layer;
            break;
        }
    }
    if (!addrLayer || addrLayer->features.isEmpty())
        return {};

    const double n = std::pow(2.0, QueryZoom);
    const double extent = addrLayer->extent;

    // Find nearest address point
    double minDist = std::numeric_limits<double>::max();
    const VectorTile::Feature *nearest = nullptr;

    for (const auto &feature : addrLayer->features) {
        if (feature.type != 1) // POINT only
            continue;

        QPointF pt = VectorTile::decodePoint(feature.geometry);

        double ptLon = (tileX + pt.x() / extent) / n * 360.0 - 180.0;
        double yMerc = 1.0 - (tileY + 1.0 - pt.y() / extent) / n;
        double ptLat = std::atan(std::sinh(M_PI * (1.0 - 2.0 * yMerc))) * 180.0 / M_PI;

        double dLon = lon - ptLon;
        double dLat = lat - ptLat;
        double dist = dLon * dLon + dLat * dLat;

        if (dist < minDist) {
            minDist = dist;
            nearest = &feature;
        }
    }

    if (!nearest)
        return {};

    // Build label from address properties (see osm-tiles/tilemaker/process.lua).
    // Compact local form "<street> <number>, <postcode> <city>", matching what
    // sunshine sends. city/postcode come from addr:* on the point and aren't
    // always tagged, so degrade gracefully to just the street (or name).
    QString street = nearest->properties.value(QStringLiteral("street"));
    QString houseNumber = nearest->properties.value(QStringLiteral("housenumber"));
    QString name = nearest->properties.value(QStringLiteral("name"));
    QString city = nearest->properties.value(QStringLiteral("city"));
    QString postcode = nearest->properties.value(QStringLiteral("postcode"));

    QString streetPart;
    if (!street.isEmpty())
        streetPart = houseNumber.isEmpty() ? street : street + QStringLiteral(" ") + houseNumber;
    else
        streetPart = name;

    QString cityPart;
    if (!city.isEmpty())
        cityPart = postcode.isEmpty() ? city : postcode + QStringLiteral(" ") + city;

    if (!streetPart.isEmpty() && !cityPart.isEmpty())
        return streetPart + QStringLiteral(", ") + cityPart;
    return streetPart.isEmpty() ? cityPart : streetPart;
}

QVariantList RoadInfoService::streetsInBbox(double minLat, double minLon,
                                              double maxLat, double maxLon)
{
    QVariantList result;
    if (!m_dbOpen)
        return result;

    // Tile range. latToTileY() returns TMS Y (Y=0 at bottom), so larger lat
    // maps to larger tile Y.
    int txMin = lonToTileX(minLon, QueryZoom);
    int txMax = lonToTileX(maxLon, QueryZoom);
    int tyMin = latToTileY(minLat, QueryZoom);
    int tyMax = latToTileY(maxLat, QueryZoom);
    if (txMin > txMax) std::swap(txMin, txMax);
    if (tyMin > tyMax) std::swap(tyMin, tyMax);

    const double n = std::pow(2.0, QueryZoom);

    for (int tx = txMin; tx <= txMax; ++tx) {
        for (int ty = tyMin; ty <= tyMax; ++ty) {
            quint64 cacheKey = (static_cast<quint64>(tx) << 32)
                               | static_cast<quint64>(static_cast<uint32_t>(ty));

            VectorTile::Tile *tile = nullptr;
            if (m_tileCache.contains(cacheKey)) {
                tile = &m_tileCache[cacheKey];
                m_cacheOrder.removeOne(cacheKey);
                m_cacheOrder.append(cacheKey);
            } else {
                QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
                QSqlQuery query(db);
                query.prepare(QStringLiteral(
                    "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
                query.addBindValue(QueryZoom);
                query.addBindValue(tx);
                query.addBindValue(ty);

                if (!query.exec() || !query.next())
                    continue;

                QByteArray tileData = query.value(0).toByteArray();
                QByteArray decompressed = VectorTile::gunzip(tileData);
                if (decompressed.isEmpty())
                    continue;

                while (m_cacheOrder.size() >= MaxCachedTiles) {
                    quint64 evict = m_cacheOrder.takeFirst();
                    m_tileCache.remove(evict);
                }

                m_tileCache.insert(cacheKey, VectorTile::parse(decompressed));
                m_cacheOrder.append(cacheKey);
                tile = &m_tileCache[cacheKey];
            }

            // Find streets layer
            const VectorTile::Layer *streetsLayer = nullptr;
            for (const auto &layer : tile->layers) {
                if (layer.name == QLatin1String("streets")) {
                    streetsLayer = &layer;
                    break;
                }
            }
            if (!streetsLayer || streetsLayer->features.isEmpty())
                continue;

            const double extent = streetsLayer->extent;

            for (const auto &feature : streetsLayer->features) {
                if (feature.type != 2) // LINESTRING only
                    continue;

                QString kind = feature.properties.value(QStringLiteral("kind"));
                QString roundaboutStr = feature.properties.value(
                    QStringLiteral("junction_roundabout"));
                bool isRoundabout = (roundaboutStr == QLatin1String("true") ||
                                     roundaboutStr == QLatin1String("1"));

                // Filter to vehicle road types or roundabouts.
                if (!s_roadTypes.contains(kind) && !isRoundabout)
                    continue;

                const QVector<QVector<QPointF>> parts =
                    VectorTile::decodeLineStringParts(feature.geometry);
                if (parts.isEmpty())
                    continue;

                const QString name = feature.properties.value(QStringLiteral("name"));

                // One entry per part: a multipart feature is several disjoint
                // stretches of the same road, and joining them would draw a
                // line across whatever sits between.
                for (const QVector<QPointF> &tilePoints : parts) {
                    QVariantList points;
                    points.reserve(tilePoints.size());
                    double fMinLat = std::numeric_limits<double>::max();
                    double fMaxLat = -std::numeric_limits<double>::max();
                    double fMinLon = std::numeric_limits<double>::max();
                    double fMaxLon = -std::numeric_limits<double>::max();

                    for (const auto &tp : tilePoints) {
                        double lon = (tx + tp.x() / extent) / n * 360.0 - 180.0;
                        double yMerc = 1.0 - (ty + 1.0 - tp.y() / extent) / n;
                        double lat = std::atan(std::sinh(M_PI * (1.0 - 2.0 * yMerc)))
                                     * 180.0 / M_PI;
                        QVariantList pt;
                        pt << lat << lon;
                        points.append(QVariant(pt));
                        fMinLat = std::min(fMinLat, lat);
                        fMaxLat = std::max(fMaxLat, lat);
                        fMinLon = std::min(fMinLon, lon);
                        fMaxLon = std::max(fMaxLon, lon);
                    }

                    // Bbox intersection test.
                    if (fMaxLat < minLat || fMinLat > maxLat ||
                        fMaxLon < minLon || fMinLon > maxLon)
                        continue;

                    QVariantMap entry;
                    entry[QStringLiteral("points")] = points;
                    entry[QStringLiteral("kind")] = kind;
                    entry[QStringLiteral("roundabout")] = isRoundabout;
                    entry[QStringLiteral("name")] = name;
                    result.append(entry);
                }
            }
        }
    }

    return result;
}
