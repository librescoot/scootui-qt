#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVariantList>
#include "VectorTileDecoder.h"

// Inputs the road matcher needs from main-thread state, captured at request
// time so the worker never touches a store.
struct RoadMatchRequest {
    double lat = 0;
    double lon = 0;
    double courseDegrees = 0;
    bool headingReliable = false;
    double maxDistanceMeters = 35.0;
    QString previousMatchKey;
    QString routeName;
};

struct RoadMatchResult {
    // False when the mbtiles is not open (e.g. cold boot before /data
    // mounts); the facade then neither publishes nor counts a miss.
    bool dbAvailable = false;
    bool matched = false;
    bool confident = false;
    QString name;
    QString refs;
    QString kind;
    QString maxspeed;
    QString key;
    double bearingDegrees = -1;
    double lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    double distanceMeters = 0;
};

// The tile engine behind RoadInfoService, living on a worker thread: owns the
// mbtiles SQLite connection (thread-affine) and the decoded-tile LRU cache,
// and runs every query that touches them. Methods must only ever run on the
// worker thread; RoadInfoService is the sole caller and dispatches via
// QMetaObject::invokeMethod.
class RoadInfoWorker : public QObject
{
    Q_OBJECT

public:
    RoadInfoWorker();
    ~RoadInfoWorker() override;

    // Idempotent open/reopen keyed on path AND mtime: an OTA map install
    // replaces map.mbtiles at the same path with a new inode, so a path-only
    // check would keep serving from the stale (unlinked) fd and pin its disk
    // space until restart. When the same file is already open this keeps the
    // connection and the tile LRU cache, so it is safe to call from the
    // availability poller / file watcher on every routing flap.
    void reload();
    // Close the connection on the worker thread; must run before the thread
    // stops so the QSqlDatabase teardown happens on its owning thread.
    void closeDb();

    RoadMatchResult matchRoad(const RoadMatchRequest &request);
    QString lookupNearestAddress(double lat, double lon);
    QVariantList streetsInBbox(double minLat, double minLon,
                               double maxLat, double maxLon);

private:
    // Cheap self-heal for the cold-boot race: scootui can start before /data
    // is mounted, so the mbtiles may appear only later.
    void openIfNeeded();
    const VectorTile::Tile *tileFor(int tileX, int tileY);
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);

    bool m_dbOpen = false;
    QString m_dbConnectionName;
    QString m_dbPath;
    QDateTime m_dbMtime;

    // Tile cache (LRU)
    QHash<quint64, VectorTile::Tile> m_tileCache;
    QList<quint64> m_cacheOrder; // oldest first

    static constexpr int QueryZoom = 14;
    static constexpr int MaxCachedTiles = 50;
};
