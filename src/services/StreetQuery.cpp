#include "StreetQueryDispatcher.h"
#include "TileLoader.h"
#include <QThread>
#include <QSet>
#include <QtMath>
#include <limits>
#include <algorithm>

static const QSet<QString> s_roadTypes = {
    QStringLiteral("motorway"), QStringLiteral("trunk"),
    QStringLiteral("primary"), QStringLiteral("secondary"),
    QStringLiteral("tertiary"), QStringLiteral("unclassified"),
    QStringLiteral("residential"), QStringLiteral("living_street"),
    QStringLiteral("service")
};

static constexpr int QueryZoom = 14;
static int lonToTileX(double lon, int zoom)
{
    return int(std::floor((lon + 180.0) / 360.0 * (1 << zoom)));
}
static int latToTileY(double lat, int zoom)
{
    const double r = qDegreesToRadians(lat);
    return (1 << zoom) - 1 - int(std::floor(
        (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / M_PI) / 2.0 * (1 << zoom)));
}

StreetQueryResult queryStreets(const StreetQueryRequest &request)
{
    StreetQueryResult output;
    auto &result = output.streets;
    const auto minLat = request.minLat, minLon = request.minLon;
    const auto maxLat = request.maxLat, maxLon = request.maxLon;
    // This API is for local icons, never an arbitrary map export.
    if (!std::isfinite(minLat) || !std::isfinite(maxLat)
        || !std::isfinite(minLon) || !std::isfinite(maxLon)
        || minLat < -85 || maxLat > 85 || minLon < -180 || maxLon >= 180
        || minLat > maxLat || minLon > maxLon || request.path.isEmpty())
        return output;

    // Tile range. latToTileY() returns TMS Y (Y=0 at bottom), so larger lat
    // maps to larger tile Y.
    int txMin = lonToTileX(minLon, QueryZoom);
    int txMax = lonToTileX(maxLon, QueryZoom);
    int tyMin = latToTileY(minLat, QueryZoom);
    int tyMax = latToTileY(maxLat, QueryZoom);
    if (txMin > txMax) std::swap(txMin, txMax);
    if (tyMin > tyMax) std::swap(tyMin, tyMax);

    if (txMax - txMin > 2 || tyMax - tyMin > 2)
        return output;
    TileLoader loader;
    loader.setPath(request.path, request.mapGeneration);
    output.complete = true;
    int pointCount = 0;
    const double n = std::pow(2.0, QueryZoom);

    for (int tx = txMin; tx <= txMax; ++tx) {
        for (int ty = tyMin; ty <= tyMax; ++ty) {
            quint64 cacheKey = (static_cast<quint64>(tx) << 32)
                               | static_cast<quint64>(static_cast<uint32_t>(ty));

            if (QThread::currentThread()->isInterruptionRequested())
                return {};
            const auto tile = loader.read(cacheKey, QueryZoom, request.mapGeneration,
                                          4 * 1024 * 1024, 16 * 1024 * 1024);
            if (!tile) {
                output.complete = false;
                continue;
            }

            // Find streets layer
            const VectorTile::Layer *streetsLayer = nullptr;
            for (const auto &layer : tile->layers) {
                if (layer.name == QLatin1String("streets")) {
                    streetsLayer = &layer;
                    break;
                }
            }
            if (!streetsLayer || !streetsLayer->extent || streetsLayer->features.isEmpty())
                continue;

            const double extent = streetsLayer->extent;

            for (const auto &feature : streetsLayer->features) {
                if (QThread::currentThread()->isInterruptionRequested())
                    return {};
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

                    // Reject oversized output as a whole, not a misleading partial ring.
                    pointCount += points.size();
                    if (pointCount > 32768 || result.size() >= 2048)
                        return {};
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

    return output;
}
