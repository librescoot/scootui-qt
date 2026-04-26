#include "RoadInfoService.h"
#include "AddressDatabaseService.h"
#include "NavigationService.h"
#include "stores/GpsStore.h"
#include "stores/SpeedLimitStore.h"

#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QVariantMap>
#include <QtMath>
#include <algorithm>

static const QSet<QString> s_roadTypes = {
    QStringLiteral("motorway"), QStringLiteral("trunk"),
    QStringLiteral("primary"), QStringLiteral("secondary"),
    QStringLiteral("tertiary"), QStringLiteral("unclassified"),
    QStringLiteral("residential"), QStringLiteral("living_street"),
    QStringLiteral("service")
};

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
            qDebug() << "RoadInfoService: mbtiles database opened";
        } else {
            qWarning() << "RoadInfoService: failed to open mbtiles";
        }
    }

    if (m_dbOpen) {
        connect(gps, &GpsStore::latitudeChanged, this, &RoadInfoService::onGpsChanged);
        connect(gps, &GpsStore::longitudeChanged, this, &RoadInfoService::onGpsChanged);
    }
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

void RoadInfoService::reloadMbtiles()
{
    QString path = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : AddressDatabaseService::MbtilesPath;

    if (!QFile::exists(path))
        return;

    // Close existing connection if open
    if (m_dbOpen) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_dbConnectionName);
        m_dbOpen = false;
        m_tileCache.clear();
        m_cacheOrder.clear();
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  m_dbConnectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (db.open()) {
        m_dbOpen = true;
        qDebug() << "RoadInfoService: mbtiles database opened";
        if (!m_gps->property("latitude").isValid()) {
            // GPS signals might already be connected; only connect if not yet done
        }
        // Ensure GPS signals are connected
        disconnect(m_gps, &GpsStore::latitudeChanged, this, &RoadInfoService::onGpsChanged);
        disconnect(m_gps, &GpsStore::longitudeChanged, this, &RoadInfoService::onGpsChanged);
        connect(m_gps, &GpsStore::latitudeChanged, this, &RoadInfoService::onGpsChanged);
        connect(m_gps, &GpsStore::longitudeChanged, this, &RoadInfoService::onGpsChanged);
    } else {
        qWarning() << "RoadInfoService: failed to open mbtiles";
    }
}

void RoadInfoService::countMissAndMaybeClear()
{
    if (++m_consecutiveMisses >= ClearAfterMisses) {
        m_speedLimit->setRoadNameDirect(QString());
        m_speedLimit->setRoadTypeDirect(QString());
        m_speedLimit->setSpeedLimitDirect(QString());
        m_speedLimit->setRoadBearingDirect(-1);
    }
}

void RoadInfoService::onGpsChanged()
{
    if (!m_gps || !m_gps->hasGpsFix()) {
        m_consecutiveMisses = ClearAfterMisses;
        m_speedLimit->setRoadNameDirect(QString());
        m_speedLimit->setRoadTypeDirect(QString());
        m_speedLimit->setSpeedLimitDirect(QString());
        m_speedLimit->setRoadBearingDirect(-1);
        return;
    }

    if (m_lastUpdate.elapsed() < ThrottleMs)
        return;

    m_lastUpdate.restart();
    updateRoadInfo(m_gps->latitude(), m_gps->longitude());
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
    // Route-driven path: when Valhalla's per-edge attributes are available
    // for the segment we're on, they are the authoritative source — same
    // edges that decided the route, posted speed limit straight from the
    // routing graph, tunnel/bridge state from the graph topology rather
    // than from a 2D tile geometry race that can flip with sub-meter GPS
    // noise. Bearing isn't set here; MapService uses routeSegmentBearing()
    // when navigating, so the tile-derived roadBearing is unused anyway.
    if (m_navigation && m_navigation->hasCurrentEdgeAttrs()) {
        m_consecutiveMisses = 0;
        m_speedLimit->setRoadNameDirect(m_navigation->currentEdgeName());
        m_speedLimit->setRoadTypeDirect(m_navigation->currentEdgeRoadClass());
        int kph = m_navigation->currentEdgeSpeedLimitKph();
        m_speedLimit->setSpeedLimitDirect(
            kph > 0 ? QString::number(kph) : QString());
        return;
    }

    if (!m_dbOpen)
        return;

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

        if (!query.exec() || !query.next()) {
            countMissAndMaybeClear();
            return;
        }

        QByteArray tileData = query.value(0).toByteArray();
        QByteArray decompressed = VectorTile::gunzip(tileData);
        if (decompressed.isEmpty()) {
            countMissAndMaybeClear();
            return;
        }

        // Evict oldest if cache full
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
    if (!streetsLayer || streetsLayer->features.isEmpty()) {
        countMissAndMaybeClear();
        return;
    }

    const double n = std::pow(2.0, QueryZoom);
    const double extent = streetsLayer->extent;

    // Build candidate list: for each linestring feature whose kind passes the
    // vehicle-road filter, find the closest segment to the GPS point and its
    // distance. We keep all candidates so a route-aware second pass can pick
    // by name match instead of pure geometry — overlapping ways at the same
    // lat/lon (Tiergarten Tunnel under Straße des 17. Juni) make the bare
    // nearest-segment race effectively random.
    struct Candidate {
        const VectorTile::Feature *feature = nullptr;
        double dist = std::numeric_limits<double>::max();
        double segLat1 = 0, segLon1 = 0, segLat2 = 0, segLon2 = 0;
        bool isTunnel = false;
    };
    QList<Candidate> candidates;

    for (const auto &feature : streetsLayer->features) {
        if (feature.type != 2) // LINESTRING only
            continue;

        // Filter to vehicle road types
        auto kindIt = feature.properties.constFind(QStringLiteral("kind"));
        if (kindIt == feature.properties.constEnd() || !s_roadTypes.contains(kindIt.value()))
            continue;

        QVector<QPointF> tilePoints = VectorTile::decodeLineString(feature.geometry);

        Candidate cand;
        cand.feature = &feature;
        cand.isTunnel = (feature.properties.value(QStringLiteral("tunnel")) == QLatin1String("true"));

        for (int i = 0; i < tilePoints.size() - 1; ++i) {
            // Convert tile coordinates to geographic (TMS: Y flipped within tile)
            double lon1 = (tileX + tilePoints[i].x() / extent) / n * 360.0 - 180.0;
            double yMerc1 = 1.0 - (tileY + 1.0 - tilePoints[i].y() / extent) / n;
            double lat1 = std::atan(std::sinh(M_PI * (1.0 - 2.0 * yMerc1))) * 180.0 / M_PI;

            double lon2 = (tileX + tilePoints[i + 1].x() / extent) / n * 360.0 - 180.0;
            double yMerc2 = 1.0 - (tileY + 1.0 - tilePoints[i + 1].y() / extent) / n;
            double lat2 = std::atan(std::sinh(M_PI * (1.0 - 2.0 * yMerc2))) * 180.0 / M_PI;

            // Point-to-segment distance (Euclidean in lat/lon, sufficient for nearest)
            double dx = lon2 - lon1;
            double dy = lat2 - lat1;
            double px = lon - lon1;
            double py = lat - lat1;
            double denom = dx * dx + dy * dy;
            if (denom < 1e-15)
                continue;

            double t = std::clamp((px * dx + py * dy) / denom, 0.0, 1.0);
            double closestLon = lon1 + t * dx;
            double closestLat = lat1 + t * dy;

            double distLon = lon - closestLon;
            double distLat = lat - closestLat;
            double dist = distLon * distLon + distLat * distLat;

            if (dist < cand.dist) {
                cand.dist = dist;
                cand.segLat1 = lat1; cand.segLon1 = lon1;
                cand.segLat2 = lat2; cand.segLon2 = lon2;
            }
        }

        if (cand.feature && cand.dist < std::numeric_limits<double>::max())
            candidates.append(cand);
    }

    if (candidates.isEmpty()) {
        countMissAndMaybeClear();
        return;
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.dist < b.dist; });

    const Candidate *chosen = nullptr;

    // Route-aware: when navigating, prefer the closest candidate whose name
    // matches the road we're currently driving along (per the route's last
    // passed maneuver). This is what fixes the Tiergarten case — the tunnel
    // and the surface road overlap geometrically, but only one matches the
    // route's current edge name.
    QString routeName = m_navigation
        ? m_navigation->currentSegmentStreetName() : QString();
    if (!routeName.isEmpty()) {
        for (const auto &cand : candidates) {
            const QString candName = cand.feature->properties.value(QStringLiteral("name"));
            if (!candName.isEmpty() &&
                candName.compare(routeName, Qt::CaseInsensitive) == 0) {
                chosen = &cand;
                break; // candidates are dist-sorted, first match is nearest
            }
        }
    }

    // Fallback: nearest, but if that nearest is a tunnel, prefer a non-tunnel
    // alternative within ~2× its distance. Surface and tunnel ways at the
    // same location project to virtually identical 2D distances; without this
    // tie-break the choice flips with sub-meter GPS noise.
    if (!chosen) {
        chosen = &candidates.first();
        if (chosen->isTunnel) {
            for (const auto &cand : candidates) {
                if (!cand.isTunnel && cand.dist < chosen->dist * 2.0) {
                    chosen = &cand;
                    break;
                }
            }
        }
    }

    m_consecutiveMisses = 0;

    QString name = chosen->feature->properties.value(QStringLiteral("name"));
    QString kind = chosen->feature->properties.value(QStringLiteral("kind"));
    QString maxspeed = chosen->feature->properties.value(QStringLiteral("maxspeed"));

    m_speedLimit->setSpeedLimitDirect(maxspeed);
    m_speedLimit->setRoadNameDirect(name);
    m_speedLimit->setRoadTypeDirect(kind);

    // Compute bearing of chosen segment
    double dLon = (chosen->segLon2 - chosen->segLon1) * M_PI / 180.0;
    double y = std::sin(dLon) * std::cos(chosen->segLat2 * M_PI / 180.0);
    double x = std::cos(chosen->segLat1 * M_PI / 180.0) * std::sin(chosen->segLat2 * M_PI / 180.0) -
               std::sin(chosen->segLat1 * M_PI / 180.0) * std::cos(chosen->segLat2 * M_PI / 180.0) * std::cos(dLon);
    double bearing = (std::abs(x) < 1e-10 && std::abs(y) < 1e-10)
        ? -1.0
        : std::fmod(std::atan2(y, x) * 180.0 / M_PI + 360.0, 360.0);
    m_speedLimit->setRoadBearingDirect(bearing);
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

    // Build label from address properties (see osm-tiles/tilemaker/process.lua)
    QString street = nearest->properties.value(QStringLiteral("street"));
    QString houseNumber = nearest->properties.value(QStringLiteral("housenumber"));
    QString name = nearest->properties.value(QStringLiteral("name"));

    if (!street.isEmpty()) {
        if (!houseNumber.isEmpty())
            return street + QStringLiteral(" ") + houseNumber;
        return street;
    }

    return name;
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

                QVector<QPointF> tilePoints = VectorTile::decodeLineString(feature.geometry);
                if (tilePoints.isEmpty())
                    continue;

                // Decode to lat/lon and compute feature bbox for quick filter.
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

                QString name = feature.properties.value(QStringLiteral("name"));

                QVariantMap entry;
                entry[QStringLiteral("points")] = points;
                entry[QStringLiteral("kind")] = kind;
                entry[QStringLiteral("roundabout")] = isRoundabout;
                entry[QStringLiteral("name")] = name;
                result.append(entry);
            }
        }
    }

    return result;
}
