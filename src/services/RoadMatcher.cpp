#include "RoadMatcher.h"
#include "RoadMatchSearchBounds.h"
#include <QSet>
#include <QThread>
#include <QtMath>

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

namespace {
int lonToTileX(double lon, int zoom)
{
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

int latToTileY(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    double n = std::pow(2.0, zoom);
    int slippyY = static_cast<int>(std::floor(
        (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * n));
    // MBTiles uses TMS (Y=0 at bottom), convert from slippy (Y=0 at top)
    return static_cast<int>(n) - 1 - slippyY;
}

}

RoadMatchResult matchRoad(const RoadMatchRequest &request)
{
    constexpr int QueryZoom = 14;
    const double lat = request.lat, lon = request.lon;
    QList<RoadMatchCandidate> candidates;
    const bool waitingForTiles = request.waitingForTiles;

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
            const auto it = request.tiles.constFind(cacheKey);
            if (it == request.tiles.cend())
                continue;
            const VectorTile::Tile &tile = it.value();
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
            const auto searchBounds = RoadMatchSearchBounds::around(
                lat, lon, tileX, tileY, QueryZoom, extent);

            for (const auto &feature : streetsLayer->features) {
                if (QThread::currentThread()->isInterruptionRequested())
                    return {};
                if (feature.type != 2)
                    continue;
                const QString kind =
                    feature.properties.value(QStringLiteral("kind"));
                if (!s_roadTypes.contains(kind))
                    continue;
                const QVector<QVector<QPointF>> parts =
                    VectorTile::decodeLineStringParts(feature.geometry);
                if (QThread::currentThread()->isInterruptionRequested())
                    return {};
                if (!searchBounds.intersects(parts))
                    continue;

                RoadMatchCandidate candidate;
                candidate.name =
                    feature.properties.value(QStringLiteral("name"));
                candidate.kind = kind;
                candidate.routeNetworks =
                    feature.properties.value(QStringLiteral("route_networks"));
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
                        if ((i & 255) == 0
                            && QThread::currentThread()->isInterruptionRequested())
                            return {};
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

    if (candidates.isEmpty() && waitingForTiles)
        return {{}, {}, true};

    QList<RoadMatchCandidateScore> policyCandidates;
    policyCandidates.reserve(candidates.size());
    const QString &routeName = request.routeName;
    for (const RoadMatchCandidate &candidate : candidates) {
        auto score = candidate.policy;
        if (!routeName.isEmpty()
            && candidate.name.compare(routeName, Qt::CaseInsensitive) == 0) {
            score.distanceMeters = std::max(0.0, score.distanceMeters - 12.0);
        }
        policyCandidates.append(score);
    }
    if (QThread::currentThread()->isInterruptionRequested())
        return {};
    const RoadMatchSelection selection = RoadMatchPolicy::select(
        policyCandidates, request.heading, request.headingReliable,
        request.previousKey, request.maxDistance);
    RoadMatchResult result;
    result.selection = selection;
    if (selection.index >= 0)
        result.chosen = candidates[selection.index];
    return result;
}
