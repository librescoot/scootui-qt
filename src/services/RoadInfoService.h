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

private slots:
    void onGpsChanged();

private:
    void updateRoadInfo(double lat, double lon);
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
};
