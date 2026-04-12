#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include "VectorTileDecoder.h"

class GpsStore;
class SpeedLimitStore;

class RoadInfoService : public QObject
{
    Q_OBJECT

public:
    explicit RoadInfoService(GpsStore *gps, SpeedLimitStore *speedLimit,
                              QObject *parent = nullptr);
    ~RoadInfoService();

    void reloadMbtiles();

    // Look up the nearest address label from the offline addresses tile layer
    QString lookupNearestAddress(double lat, double lon);

private slots:
    void onGpsChanged();

private:
    void updateRoadInfo(double lat, double lon);
    void countMissAndMaybeClear();
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);

    GpsStore *m_gps;
    SpeedLimitStore *m_speedLimit;

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
