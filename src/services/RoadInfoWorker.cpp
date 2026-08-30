#include "RoadInfoWorker.h"
#include "AddressDatabaseService.h"
#include "RoadMatchPolicy.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
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

RoadInfoWorker::RoadInfoWorker()
    : m_dbConnectionName(QStringLiteral("roadinfo_tiles"))
{
}

RoadInfoWorker::~RoadInfoWorker()
{
    // closeDb must already have run on the worker thread; the connection is
    // thread-affine, so closing here (main thread, after the thread stopped)
    // would be too late.
}

void RoadInfoWorker::openIfNeeded()
{
    if (!m_dbOpen)
        reload();
}

void RoadInfoWorker::reload()
{
    // Prefer local map.mbtiles (desktop/simulator), fall back to device path
    const QString path = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : AddressDatabaseService::MbtilesPath;

    if (!QFile::exists(path))
        return;

    const QDateTime mtime = QFileInfo(path).lastModified();
    if (m_dbOpen && path == m_dbPath && mtime == m_dbMtime)
        return;

    closeDb();

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

void RoadInfoWorker::closeDb()
{
    if (!m_dbOpen)
        return;
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

int RoadInfoWorker::lonToTileX(double lon, int zoom)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

int RoadInfoWorker::latToTileY(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    double n = std::pow(2.0, zoom);
    int slippyY = static_cast<int>(std::floor(
        (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n));
    // MBTiles uses TMS (Y=0 at bottom), convert from slippy (Y=0 at top)
    return static_cast<int>(n) - 1 - slippyY;
}

const VectorTile::Tile *RoadInfoWorker::tileFor(int tileX, int tileY)
{
    const quint64 cacheKey = (static_cast<quint64>(tileX) << 32)
        | static_cast<quint64>(static_cast<uint32_t>(tileY));
    if (m_tileCache.contains(cacheKey)) {
        m_cacheOrder.removeOne(cacheKey);
        m_cacheOrder.append(cacheKey);
        return &m_tileCache[cacheKey];
    }

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
    query.addBindValue(QueryZoom);
    query.addBindValue(tileX);
    query.addBindValue(tileY);
    if (!query.exec() || !query.next())
        return nullptr;

    const QByteArray decompressed =
        VectorTile::gunzip(query.value(0).toByteArray());
    if (decompressed.isEmpty())
        return nullptr;

    while (m_cacheOrder.size() >= MaxCachedTiles) {
        const quint64 evict = m_cacheOrder.takeFirst();
        m_tileCache.remove(evict);
    }
    m_tileCache.insert(cacheKey, VectorTile::parse(decompressed));
    m_cacheOrder.append(cacheKey);
    return &m_tileCache[cacheKey];
}

RoadMatchResult RoadInfoWorker::matchRoad(const RoadMatchRequest &request)
{
    openIfNeeded();
    if (!m_dbOpen)
        return {};

    RoadMatchResult miss;
    miss.dbAvailable = true;

    const double lat = request.lat;
    const double lon = request.lon;

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

            const VectorTile::Tile *tile = tileFor(tileX, tileY);
            if (!tile)
                continue;
            const VectorTile::Layer *streetsLayer = nullptr;
            for (const auto &layer : tile->layers) {
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

    QList<RoadMatchCandidateScore> policyCandidates;
    policyCandidates.reserve(candidates.size());
    for (const Candidate &candidate : candidates) {
        auto score = candidate.policy;
        if (!request.routeName.isEmpty()
            && candidate.name.compare(request.routeName,
                                      Qt::CaseInsensitive) == 0) {
            score.distanceMeters = std::max(0.0, score.distanceMeters - 12.0);
        }
        policyCandidates.append(score);
    }
    const RoadMatchSelection selection = RoadMatchPolicy::select(
        policyCandidates, request.courseDegrees, request.headingReliable,
        request.previousMatchKey, request.maxDistanceMeters);
    if (selection.index < 0)
        return miss;

    const Candidate &chosen = candidates[selection.index];
    RoadMatchResult result;
    result.dbAvailable = true;
    result.matched = true;
    result.confident = selection.confident;
    result.name = chosen.name;
    result.refs = chosen.refs;
    result.kind = chosen.kind;
    result.maxspeed = chosen.maxspeed;
    result.key = chosen.policy.key;
    result.bearingDegrees = chosen.policy.bearingDegrees;
    result.lat1 = chosen.lat1; result.lon1 = chosen.lon1;
    result.lat2 = chosen.lat2; result.lon2 = chosen.lon2;
    result.distanceMeters = chosen.actualDistanceMeters;
    return result;
}

QString RoadInfoWorker::lookupNearestAddress(double lat, double lon)
{
    openIfNeeded();
    if (!m_dbOpen)
        return {};

    const VectorTile::Tile *tile =
        tileFor(lonToTileX(lon, QueryZoom), latToTileY(lat, QueryZoom));
    if (!tile)
        return {};

    const int tileX = lonToTileX(lon, QueryZoom);
    const int tileY = latToTileY(lat, QueryZoom);

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

QVariantList RoadInfoWorker::streetsInBbox(double minLat, double minLon,
                                           double maxLat, double maxLon)
{
    QVariantList result;
    openIfNeeded();
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
            const VectorTile::Tile *tile = tileFor(tx, ty);
            if (!tile)
                continue;

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
