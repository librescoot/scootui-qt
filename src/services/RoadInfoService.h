#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QVariantList>
#include <QSet>
#include <QThread>
#include <QTimer>
#include "VectorTileDecoder.h"
#include "NavigationCadence.h"
#include "RoadMatchPolicy.h"

class GpsStore;
class SpeedLimitStore;
class NavigationService;
class MapService;
class TileLoader;
class RoadMatchDispatcher;
class StreetQueryDispatcher;
struct RoadMatchRequest;
struct RoadMatchResult;
struct StreetQueryRequest;

class RoadInfoService : public QObject
{
    Q_OBJECT

public:
    explicit RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                              NavigationService *navigation,
                              QObject *parent = nullptr);
    ~RoadInfoService();

    void reloadMbtiles();
    void stopWorkers();
    void setMapService(MapService *map);

    bool hasConfidentRoadMatch() const { return m_hasConfidentRoadMatch; }
    double matchedSegmentLat1() const { return m_matchLat1; }
    double matchedSegmentLon1() const { return m_matchLon1; }
    double matchedSegmentLat2() const { return m_matchLat2; }
    double matchedSegmentLon2() const { return m_matchLon2; }
    double roadMatchDistanceMeters() const { return m_matchDistanceMeters; }

    // Look up the nearest address label from the offline addresses tile layer
    QString lookupNearestAddress(double lat, double lon);

    // Async, latest-wins across icon instances. No QML objects cross threads.
    Q_INVOKABLE quint64 requestStreets(QObject *owner, const QVariantMap &geometry);
    Q_INVOKABLE QVariantMap cachedStreets(const QVariantMap &geometry) const;
    Q_INVOKABLE void cancelStreets(QObject *owner);

signals:
    void roadMatchChanged();
    void streetsReady(QObject *owner, quint64 sequence, const QVariantList &streets, bool complete);
    void streetsInvalidated(bool routeChanged);

private slots:
    void onGpsChanged();
    void onVehiclePositionChanged();
    void onTileLoaded(quint64 key, const VectorTile::Tile &tile, int generation);
    void onTileMissing(quint64 key, int generation);

private:
    friend class RoadInfoServiceTest;
    void checkTileFreshness(qint64 nowMs);
    StreetQueryRequest streetRequest(const QVariantMap &geometry) const;
    void invalidateStreets(bool routeChanged = false);
    void clearTileOutputs();
    void updateRoadInfo(double lat, double lon);
    void applyMatch(const RoadMatchRequest &request, const RoadMatchResult &result);
    void publishCurrentRouteAttrs();
    void onRouteContextChanged();
    void invalidatePosition();
    bool openDb(const QString &path);
    void closeDb();
    void requestTile(quint64 key);
    bool loadTileBlocking(quint64 key);
    void insertTile(quint64 key, const VectorTile::Tile &tile);
    void touchTile(quint64 key);
    void countMissAndMaybeClear();
    void clearRoadMatch();
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);

    GpsStore *m_gps;
    SpeedLimitStore *m_speedLimit;
    NavigationService *m_navigation;
    MapService *m_map = nullptr;

    QElapsedTimer m_lastUpdate;
    QElapsedTimer m_freshnessClock;
    QTimer m_freshnessTimer;
    qint64 m_lastAcceptedMatchMs = -1;
    bool m_dbOpen = false;
    QString m_dbConnectionName;
    QString m_dbPath; // path of the currently-open mbtiles (for idempotent reload)
    QDateTime m_dbMtime; // mtime at open — detects a same-path replacement (OTA install)

    // Tile cache (LRU), filled by the loader thread for the periodic match and
    // synchronously by the on-demand lookups.
    QHash<quint64, VectorTile::Tile> m_tileCache;
    QList<quint64> m_cacheOrder; // oldest first
    QThread *m_loaderThread = nullptr;
    bool m_stopping = false;
    RoadMatchDispatcher *m_matcher = nullptr;
    StreetQueryDispatcher *m_streets = nullptr;
    QTimer m_streetPrefetchTimer;
    quint64 m_routeGeneration = 0;
    int m_routeSegmentIndex = -1;
    TileLoader *m_loader = nullptr;
    int m_generation = 0;
    QSet<quint64> m_pending;
    QSet<quint64> m_absent;
    QTimer m_rematchTimer;
    bool m_hasLastPosition = false;
    double m_lastLat = 0;
    double m_lastLon = 0;

    static constexpr int FallbackUpdateIntervalMs =
        NavigationCadence::RenderTickMs * NavigationCadence::RoadInfoEveryTicks;
    static constexpr int TileFreshnessMs =
        RoadMatchRetentionState::MissesBeforeClear * FallbackUpdateIntervalMs;
    static constexpr int QueryZoom = 14;
    static constexpr int MaxCachedTiles = 50;

    RoadMatchRetentionState m_matchRetention;
    NavigationCadence::TickDivider m_updateCadence{
        NavigationCadence::RoadInfoEveryTicks};
    QString m_previousMatchKey;
    bool m_hasConfidentRoadMatch = false;
    double m_matchLat1 = 0;
    double m_matchLon1 = 0;
    double m_matchLat2 = 0;
    double m_matchLon2 = 0;
    double m_matchDistanceMeters = 0;
};
