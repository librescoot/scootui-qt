#include "RoadInfoService.h"
#include "AddressDatabaseService.h"
#include "stores/GpsStore.h"
#include "stores/SpeedLimitStore.h"

#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
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
                                   QObject *parent)
    : QObject(parent)
    , m_gps(gps)
    , m_speedLimit(speedLimit)
    , m_dbConnectionName(QStringLiteral("roadinfo_tiles"))
{
    m_lastUpdate.start();

    const QString &path = AddressDatabaseService::MbtilesPath;
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

void RoadInfoService::onGpsChanged()
{
    if (!m_gps || m_gps->gpsState() != 2) // GpsState::FixEstablished
        return;

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
    // MBTiles uses 1-indexed TMS (Y=0 at bottom), convert from slippy (Y=0 at top)
    return static_cast<int>(n) - 1 - slippyY + 1;
}

void RoadInfoService::updateRoadInfo(double lat, double lon)
{
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

        if (!query.exec() || !query.next())
            return;

        QByteArray tileData = query.value(0).toByteArray();
        QByteArray decompressed = VectorTile::gunzip(tileData);
        if (decompressed.isEmpty())
            return;

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
    if (!streetsLayer || streetsLayer->features.isEmpty())
        return;

    const double n = std::pow(2.0, QueryZoom);
    const double extent = streetsLayer->extent;

    // Find nearest road feature
    double minDist = std::numeric_limits<double>::max();
    const VectorTile::Feature *nearestFeature = nullptr;

    for (const auto &feature : streetsLayer->features) {
        if (feature.type != 2) // LINESTRING only
            continue;

        // Filter to vehicle road types
        auto kindIt = feature.properties.constFind(QStringLiteral("kind"));
        if (kindIt == feature.properties.constEnd() || !s_roadTypes.contains(kindIt.value()))
            continue;

        QVector<QPointF> tilePoints = VectorTile::decodeLineString(feature.geometry);

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

            if (dist < minDist) {
                minDist = dist;
                nearestFeature = &feature;
            }
        }
    }

    if (!nearestFeature)
        return;

    QString name = nearestFeature->properties.value(QStringLiteral("name"));
    QString kind = nearestFeature->properties.value(QStringLiteral("kind"));
    QString maxspeed = nearestFeature->properties.value(QStringLiteral("maxspeed"));

    m_speedLimit->setSpeedLimitDirect(maxspeed);
    m_speedLimit->setRoadNameDirect(name);
    m_speedLimit->setRoadTypeDirect(kind);
}
