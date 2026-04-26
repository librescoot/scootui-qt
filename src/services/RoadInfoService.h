#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QVariantList>
#include "VectorTileDecoder.h"

class GpsStore;
class SpeedLimitStore;
class NavigationService;

class RoadInfoService : public QObject
{
    Q_OBJECT

public:
    explicit RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                              NavigationService *navigation,
                              QObject *parent = nullptr);
    ~RoadInfoService();

    void reloadMbtiles();

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

private slots:
    void onGpsChanged();

private:
    void updateRoadInfo(double lat, double lon);
    void countMissAndMaybeClear();
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);

    GpsStore *m_gps;
    SpeedLimitStore *m_speedLimit;
    NavigationService *m_navigation;

    QElapsedTimer m_lastUpdate;
    bool m_dbOpen = false;
    QString m_dbConnectionName;

    // Tile cache (LRU)
    QHash<quint64, VectorTile::Tile> m_tileCache;
    QList<quint64> m_cacheOrder; // oldest first

    static constexpr int ThrottleMs = 3000;
    static constexpr int QueryZoom = 14;
    static constexpr int MaxCachedTiles = 50;
    static constexpr int ClearAfterMisses = 3; // clear road name after N consecutive no-match results

    int m_consecutiveMisses = 0;
};
