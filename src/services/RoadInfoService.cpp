#include "RoadInfoService.h"
#include "AddressDatabaseService.h"
#include "MapService.h"
#include "NavigationService.h"
#include "RoadMatchPolicy.h"
#include "RoadMatchDispatcher.h"
#include "StreetQueryDispatcher.h"
#include "RoadWorkerThreads.h"
#include "TileLoader.h"
#include "stores/GpsStore.h"
#include "stores/SpeedLimitStore.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QVariantMap>
#include <QtMath>
#include <algorithm>
#include <limits>

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
    m_freshnessClock.start();
    // Independent of GPS, submissions and completions: a latest-wins worker
    // may reject every result under sustained overload, or GPS may go silent.
    m_freshnessTimer.setInterval(100);
    connect(&m_freshnessTimer, &QTimer::timeout, this, [this]() {
        checkTileFreshness(m_freshnessClock.elapsed());
    });
    m_freshnessTimer.start();

    m_streets = new StreetQueryDispatcher(this);
    connect(m_streets, &StreetQueryDispatcher::ready, this, &RoadInfoService::streetsReady);
    m_streetPrefetchTimer.setSingleShot(true);
    connect(&m_streetPrefetchTimer, &QTimer::timeout, this, [this]() {
        if (!m_stopping && m_navigation && m_dbOpen) {
            const auto geometry = m_navigation->currentRoundaboutRender();
            if (!geometry.isEmpty())
                m_streets->prefetch(streetRequest(geometry));
        }
    });
    if (m_navigation) {
        connect(m_navigation, &NavigationService::routeChanged,
                this, [this]() { invalidateStreets(true); });
        connect(m_navigation, &NavigationService::roundaboutRenderChanged,
                this, [this]() { invalidateStreets(true); });
    }
    m_matcher = new RoadMatchDispatcher(this);
    connect(m_matcher, &RoadMatchDispatcher::ready, this, &RoadInfoService::applyMatch);
    if (m_navigation) {
        connect(m_navigation, &NavigationService::routeChanged,
                this, &RoadInfoService::onRouteContextChanged);
        connect(m_navigation, &NavigationService::routeAttributesChanged,
                this, &RoadInfoService::onRouteContextChanged);
        connect(m_navigation, &NavigationService::positionChanged, this, [this]() {
            if (m_routeSegmentIndex != m_navigation->currentSegmentIndex())
                onRouteContextChanged();
        });
    }

    m_loaderThread = RoadWorkerThreads::create(QStringLiteral("roadinfo-tiles"));
    m_loader = new TileLoader;
    m_loader->moveToThread(m_loaderThread);
    connect(m_loaderThread, &QThread::finished, m_loader, &QObject::deleteLater);
    connect(m_loader, &TileLoader::loaded, this, &RoadInfoService::onTileLoaded);
    connect(m_loader, &TileLoader::missing, this, &RoadInfoService::onTileMissing);
    m_loaderThread->start();

    m_rematchTimer.setSingleShot(true);
    connect(&m_rematchTimer, &QTimer::timeout, this, [this]() {
        if (m_hasLastPosition)
            updateRoadInfo(m_lastLat, m_lastLon);
    });

    // Prefer local map.mbtiles (desktop/simulator), fall back to device path
    QString path = QFile::exists(QStringLiteral("map.mbtiles"))
        ? QStringLiteral("map.mbtiles")
        : AddressDatabaseService::MbtilesPath;
    if (QFile::exists(path))
        openDb(path);

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
    stopWorkers();
    closeDb();
}

void RoadInfoService::stopWorkers()
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_matcher->stop();
    m_streets->stop();
    m_streetPrefetchTimer.stop();
    m_rematchTimer.stop();
    m_freshnessTimer.stop();
    m_pending.clear();
    // Queued loader results are disconnected before its autonomous shutdown.
    disconnect(m_loader, nullptr, this, nullptr);
    m_loaderThread->requestInterruption();
    m_loaderThread->quit();
}

bool RoadInfoService::openDb(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  m_dbConnectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!db.open()) {
        qWarning() << "RoadInfoService: failed to open mbtiles";
        return false;
    }
    m_dbOpen = true;
    m_dbPath = path;
    m_dbMtime = QFileInfo(path).lastModified();
    ++m_generation;
    invalidateStreets();
    QMetaObject::invokeMethod(m_loader, "setPath", Qt::QueuedConnection,
                              Q_ARG(QString, path), Q_ARG(int, m_generation));
    if (m_hasLastPosition)
        m_rematchTimer.start(0);
    qDebug() << "RoadInfoService: mbtiles database opened";
    return true;
}

void RoadInfoService::closeDb()
{
    ++m_generation;
    invalidateStreets();
    m_matcher->invalidate();
    m_rematchTimer.stop();
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
    m_pending.clear();
    m_absent.clear();
}

void RoadInfoService::setMapService(MapService *map)
{
    if (m_map == map)
        return;
    if (m_map)
        disconnect(m_map, nullptr, this, nullptr);
    invalidatePosition();
    m_map = map;
    m_updateCadence.reset();
    if (m_map) {
        connect(m_map, &MapService::vehiclePositionChanged,
                this, &RoadInfoService::onVehiclePositionChanged);
    }
}

void RoadInfoService::reloadMbtiles()
{
    if (m_stopping)
        return;
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

    closeDb();
    openDb(path);
}

void RoadInfoService::requestTile(quint64 key)
{
    if (m_pending.contains(key) || m_absent.contains(key))
        return;
    m_pending.insert(key);
    QMetaObject::invokeMethod(m_loader, "load", Qt::QueuedConnection,
                              Q_ARG(quint64, key), Q_ARG(int, QueryZoom),
                              Q_ARG(int, m_generation));
}

void RoadInfoService::onTileLoaded(quint64 key, const VectorTile::Tile &tile, int generation)
{
    if (m_stopping || generation != m_generation)
        return;
    m_pending.remove(key);
    insertTile(key, tile);
    if (!m_rematchTimer.isActive())
        m_rematchTimer.start(0);
}

void RoadInfoService::onTileMissing(quint64 key, int generation)
{
    if (m_stopping || generation != m_generation)
        return;
    m_pending.remove(key);
    m_absent.insert(key);
}

void RoadInfoService::insertTile(quint64 key, const VectorTile::Tile &tile)
{
    while (m_cacheOrder.size() >= MaxCachedTiles) {
        const quint64 evict = m_cacheOrder.takeFirst();
        m_tileCache.remove(evict);
    }
    m_tileCache.insert(key, tile);
    m_cacheOrder.removeOne(key);
    m_cacheOrder.append(key);
}

void RoadInfoService::touchTile(quint64 key)
{
    m_cacheOrder.removeOne(key);
    m_cacheOrder.append(key);
}

bool RoadInfoService::loadTileBlocking(quint64 key)
{
    if (m_tileCache.contains(key)) {
        touchTile(key);
        return true;
    }
    if (m_absent.contains(key))
        return false;
    const int tileX = static_cast<int>(key >> 32);
    const int tileY = static_cast<int>(static_cast<uint32_t>(key & 0xffffffffu));
    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?"));
    query.addBindValue(QueryZoom);
    query.addBindValue(tileX);
    query.addBindValue(tileY);
    if (!query.exec() || !query.next()) {
        m_absent.insert(key);
        return false;
    }
    const QByteArray decompressed = VectorTile::gunzip(query.value(0).toByteArray());
    if (decompressed.isEmpty()) {
        m_absent.insert(key);
        return false;
    }
    insertTile(key, VectorTile::parse(decompressed));
    return true;
}

void RoadInfoService::countMissAndMaybeClear()
{
    // The tile matcher runs at 1 Hz. Keep the previous confident segment over
    // two isolated misses so a tile boundary, momentary ambiguity at a crossing,
    // or one noisy fix cannot pop the marker off the road. MapService still
    // releases immediately if the physical estimate moves materially away from
    // that retained segment.
    if (!m_matchRetention.retainAfterMiss())
        clearTileOutputs();
}

void RoadInfoService::checkTileFreshness(qint64 nowMs)
{
    if (m_lastAcceptedMatchMs >= 0
        && nowMs - m_lastAcceptedMatchMs >= TileFreshnessMs)
        clearTileOutputs();
}

void RoadInfoService::clearTileOutputs()
{
    m_lastAcceptedMatchMs = -1;
    m_matchRetention.reset();
    clearRoadMatch();
    m_speedLimit->clearSource(SpeedLimitStore::Source::Tile);
}

void RoadInfoService::clearRoadMatch()
{
    const bool changed = m_hasConfidentRoadMatch || !m_previousMatchKey.isEmpty();
    m_hasConfidentRoadMatch = false;
    m_previousMatchKey.clear();
    m_matchLat1 = m_matchLon1 = m_matchLat2 = m_matchLon2 = 0;
    m_matchDistanceMeters = 0;
    if (changed)
        emit roadMatchChanged();
}

void RoadInfoService::invalidatePosition()
{
    clearTileOutputs();
    m_matcher->invalidate();
    m_hasLastPosition = false;
    m_rematchTimer.stop();
}

void RoadInfoService::onRouteContextChanged()
{
    m_routeSegmentIndex = m_navigation->currentSegmentIndex();
    ++m_routeGeneration;
    m_matcher->invalidate();
    m_speedLimit->clearSource(SpeedLimitStore::Source::Route);
    if (m_navigation->hasCurrentEdgeAttrs())
        publishCurrentRouteAttrs();
    if (m_hasLastPosition)
        m_rematchTimer.start(0);
}

void RoadInfoService::onGpsChanged()
{
    if (!m_gps || !m_gps->hasValidGps())
        invalidatePosition();
    if (m_map && m_map->hasVehiclePosition())
        return;
    if (!m_gps || !m_gps->hasValidGps())
        return;

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
    if (!m_map || !m_map->hasVehiclePosition()) {
        invalidatePosition();
        return;
    }
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

void RoadInfoService::publishCurrentRouteAttrs()
{
    m_speedLimit->setRoadNameDirect(m_navigation->currentEdgeName());
    m_speedLimit->setRoadNetworksDirect(QString());
    m_speedLimit->setRoadRefsDirect(
        m_navigation->currentEdgeRefs().join(QStringLiteral(", ")));
    m_speedLimit->setRoadTypeDirect(m_navigation->currentEdgeRoadClass());
    const int kph = m_navigation->currentEdgeSpeedLimitKph();
    m_speedLimit->setSpeedLimitDirect(
        kph > 0 ? QString::number(kph) : QString());
}

void RoadInfoService::updateRoadInfo(double lat, double lon)
{
    if (m_stopping)
        return;
    m_hasLastPosition = true;
    m_lastLat = lat;
    m_lastLon = lon;
    const bool hasRouteAttrs = m_navigation
        && m_navigation->hasCurrentEdgeAttrs();
    auto publishRouteAttrs = [this]() { publishCurrentRouteAttrs(); };

    // A complete route edge is authoritative and avoids doing any tile work.
    // Partial trace attributes fall through so missing name/class/speed fields
    // can be filled from the local vector tile.
    if (hasRouteAttrs
        && !m_navigation->currentEdgeName().isEmpty()
        && !m_navigation->currentEdgeRoadClass().isEmpty()
        && m_navigation->currentEdgeSpeedLimitKph() > 0) {
        m_matcher->invalidate();
        clearTileOutputs();
        publishRouteAttrs();
        return;
    }

    if (!m_dbOpen) {
        m_matcher->invalidate();
        if (hasRouteAttrs) {
            publishRouteAttrs();
            clearRoadMatch();
        }
        return;
    }

    RoadMatchRequest request;
    request.routeGeneration = m_routeGeneration;
    request.mapGeneration = m_generation;
    request.lat = lat;
    request.lon = lon;
    const GpsSample gps = m_gps ? m_gps->currentSample() : GpsSample{};
    request.heading = gps.course;
    request.headingReliable = m_gps && gps.hasValidCoordinate()
        && gps.hasFix() && m_gps->timestampAgeMs() <= 2500
        && gps.speedKmh >= 3.0 && std::isfinite(gps.course);
    if (m_map)
        request.maxDistance = std::clamp(m_map->positionUncertaintyMeters() + 8.0,
                                         15.0, 40.0);
    else if (gps.ephMeters > 0.0)
        request.maxDistance = std::clamp(gps.ephMeters * 1.5 + 8.0, 15.0, 40.0);
    request.routeName = m_navigation
        ? m_navigation->currentSegmentStreetName() : QString();
    request.previousKey = m_previousMatchKey;
    const int centerX = lonToTileX(lon, QueryZoom);
    const int centerY = latToTileY(lat, QueryZoom);
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oy = -1; oy <= 1; ++oy) {
            const int x = centerX + ox, y = centerY + oy;
            if (x < 0 || y < 0 || x >= (1 << QueryZoom) || y >= (1 << QueryZoom))
                continue;
            const quint64 key = (quint64(x) << 32) | quint32(y);
            const auto it = m_tileCache.constFind(key);
            if (it != m_tileCache.cend()) {
                request.tiles.insert(key, it.value());
                touchTile(key);
            } else if (!m_absent.contains(key)) {
                requestTile(key);
                request.waitingForTiles = true;
            }
        }
    }
    m_matcher->submit(std::move(request));
}

void RoadInfoService::applyMatch(const RoadMatchRequest &request,
                                const RoadMatchResult &result)
{
    if (!request.isCurrent(m_matcher->latestSequence(), m_routeGeneration,
                           m_generation, m_hasLastPosition))
        return;
    if (result.waitingForTiles)
        return;
    const auto &selection = result.selection;
    const bool hasRouteAttrs = m_navigation && m_navigation->hasCurrentEdgeAttrs();
    auto publishRouteAttrs = [this]() { publishCurrentRouteAttrs(); };
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

    const auto &chosen = result.chosen;
    const bool freeDrive = !m_navigation || !m_navigation->hasRoute();
    if (freeDrive && !selection.confident) {
        // A ranked winner is not necessarily a trustworthy road at a crossing
        // or between close parallel carriageways. Retain the previous confident
        // match briefly instead of switching the marker to an ambiguous result.
        countMissAndMaybeClear();
        return;
    }

    m_matchRetention.matched();
    m_lastAcceptedMatchMs = m_freshnessClock.elapsed();
    QString name = chosen.name;
    QString refs = chosen.refs;
    QString kind = chosen.kind;
    QString routeNetworks = chosen.routeNetworks;
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
    using Source = SpeedLimitStore::Source;
    m_speedLimit->setSpeedLimitDirect(maxspeed,
        hasRouteAttrs && m_navigation->currentEdgeSpeedLimitKph() > 0 ? Source::Route : Source::Tile);
    m_speedLimit->setRoadNameDirect(name,
        hasRouteAttrs && !m_navigation->currentEdgeName().isEmpty() ? Source::Route : Source::Tile);
    m_speedLimit->setRoadNetworksDirect(routeNetworks, Source::Tile);
    m_speedLimit->setRoadRefsDirect(refs,
        hasRouteAttrs && !m_navigation->currentEdgeRefs().isEmpty() ? Source::Route : Source::Tile);
    m_speedLimit->setRoadTypeDirect(kind,
        hasRouteAttrs && !m_navigation->currentEdgeRoadClass().isEmpty() ? Source::Route : Source::Tile);
    m_speedLimit->setRoadBearingDirect(chosen.policy.bearingDegrees, Source::Tile);

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

    if (!loadTileBlocking(cacheKey))
        return {};
    const VectorTile::Tile *tile = &m_tileCache[cacheKey];

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

void RoadInfoService::invalidateStreets(bool routeChanged)
{
    m_streets->invalidate();
    if (!m_stopping) {
        emit streetsInvalidated(routeChanged);
        // Navigation updates its render value after route/instruction signals.
        // Prefetch the current pair as soon as that value settles, before the
        // QML icon's 500 m activation gate and independently of TBT creation.
        m_streetPrefetchTimer.start(0);
    }
}

StreetQueryRequest RoadInfoService::streetRequest(const QVariantMap &geometry) const
{
    StreetQueryRequest request;
    request.path = m_dbOpen ? m_dbPath : QString();
    request.mapGeneration = m_generation;
    // Serialize only for identity, on GUI: never carry QJSValue/QObject variants
    // into a worker snapshot. Geometry extraction needs just the numeric bbox.
    request.geometryKey = QJsonDocument(QJsonObject::fromVariantMap(geometry)).toJson(QJsonDocument::Compact);
    const double lat = geometry.value(QStringLiteral("centerLat")).toDouble();
    const double lon = geometry.value(QStringLiteral("centerLon")).toDouble();
    const double radius = geometry.value(QStringLiteral("ringRadius")).toDouble();
    const double reach = geometry.value(QStringLiteral("ringValid")).toBool()
        ? radius + std::max(25.0, 0.9 * radius) : std::max(radius, 12.0) * 3 + 60;
    const double dLat = reach / 111320;
    const double dLon = reach / (111320 * std::cos(qDegreesToRadians(lat)));
    request.minLat = lat - dLat; request.minLon = lon - dLon;
    request.maxLat = lat + dLat; request.maxLon = lon + dLon;
    return request;
}

quint64 RoadInfoService::requestStreets(QObject *owner, const QVariantMap &geometry)
{
    if (m_stopping || geometry.isEmpty())
        return 0;
    return m_streets->submit(owner, streetRequest(geometry));
}

QVariantMap RoadInfoService::cachedStreets(const QVariantMap &geometry) const
{
    const auto result = m_streets->cached(streetRequest(geometry));
    return {{QStringLiteral("streets"), result.streets},
            {QStringLiteral("complete"), result.complete}};
}

void RoadInfoService::cancelStreets(QObject *owner)
{
    m_streets->cancel(owner);
}
