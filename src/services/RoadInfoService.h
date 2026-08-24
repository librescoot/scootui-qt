#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QVariantList>
#include "VectorTileDecoder.h"
#include "NavigationCadence.h"

class GpsStore;
class SpeedLimitStore;
class NavigationService;
class MapService;

class RoadInfoService : public QObject
{
    Q_OBJECT

public:
    explicit RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                              NavigationService *navigation,
                              QObject *parent = nullptr);
    ~RoadInfoService();

    void reloadMbtiles();
    void setMapService(MapService *map);

    bool hasConfidentRoadMatch() const { return m_hasConfidentRoadMatch; }
    double matchedSegmentLat1() const { return m_matchLat1; }
    double matchedSegmentLon1() const { return m_matchLon1; }
    double matchedSegmentLat2() const { return m_matchLat2; }
    double matchedSegmentLon2() const { return m_matchLon2; }
    double roadMatchDistanceMeters() const { return m_matchDistanceMeters; }

    // Look up the nearest address label from the offline addresses tile layer
    QString lookupNearestAddress(double lat, double lon);

    // Return all street linestrings whose bounding box intersects the given
    // geographic bbox, at zoom QueryZoom. Each entry is a QVariantMap:
    //   { points: [[lat, lon], ...],
    //     kind: "residential"|"primary"|...,
    //     roundabout: bool,
    //     name: string }
    Q_INVOKABLE QVariantList streetsInBbox(double minLat, double minLon,
                                             double maxLat, double maxLon);

signals:
    void roadMatchChanged();

private slots:
    void onGpsChanged();
    void onVehiclePositionChanged();

private:
    void updateRoadInfo(double lat, double lon);
    void countMissAndMaybeClear();
    void clearRoadMatch();
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);

    GpsStore *m_gps;
    SpeedLimitStore *m_speedLimit;
    NavigationService *m_navigation;
    MapService *m_map = nullptr;

    QElapsedTimer m_lastUpdate;
    bool m_dbOpen = false;
    QString m_dbConnectionName;
    QString m_dbPath; // path of the currently-open mbtiles (for idempotent reload)
    QDateTime m_dbMtime; // mtime at open — detects a same-path replacement (OTA install)

    // Tile cache (LRU)
    QHash<quint64, VectorTile::Tile> m_tileCache;
    QList<quint64> m_cacheOrder; // oldest first

    static constexpr int FallbackUpdateIntervalMs =
        NavigationCadence::RenderTickMs * NavigationCadence::RoadInfoEveryTicks;
    static constexpr int QueryZoom = 14;
    static constexpr int MaxCachedTiles = 50;
    static constexpr int ClearAfterMisses = 3; // clear road name after N consecutive no-match results

    int m_consecutiveMisses = 0;
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
