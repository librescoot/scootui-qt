#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantList>
#include "NavigationCadence.h"
#include "RoadInfoWorker.h"

class GpsStore;
class SpeedLimitStore;
class NavigationService;
class MapService;

// Main-thread facade over RoadInfoWorker. Publishes road metadata to
// SpeedLimitStore and the free-drive road match to MapService; the SQLite
// tile fetch, gunzip, decode and candidate scan run on the worker thread so
// a per-GPS-sample match never stalls the UI.
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

    // Look up the nearest address label from the offline addresses tile
    // layer. Blocks the caller for the duration of the lookup (rare,
    // user-triggered), but the work runs on the worker thread so the DB
    // connection has a single home.
    QString lookupNearestAddress(double lat, double lon);

    // Return all street linestrings whose bounding box intersects the given
    // geographic bbox, at zoom QueryZoom. Each entry is a QVariantMap:
    //   { points: [[lat, lon], ...],
    //     kind: "residential"|"primary"|...,
    //     roundabout: bool,
    //     name: string }
    // Blocking like lookupNearestAddress; called once per roundabout approach.
    Q_INVOKABLE QVariantList streetsInBbox(double minLat, double minLon,
                                             double maxLat, double maxLon);

signals:
    void roadMatchChanged();

private slots:
    void onGpsChanged();
    void onVehiclePositionChanged();

private:
    void updateRoadInfo(double lat, double lon);
    void sendMatchRequest(double lat, double lon);
    void applyMatchResult(const RoadMatchResult &result);
    void processMatchResult(const RoadMatchResult &result);
    void publishRouteAttrs();
    void clearRoadInfo();
    void countMissAndMaybeClear();
    void clearRoadMatch();

    GpsStore *m_gps;
    SpeedLimitStore *m_speedLimit;
    NavigationService *m_navigation;
    MapService *m_map = nullptr;

    QThread m_thread;
    RoadInfoWorker *m_worker = nullptr;
    // One match request in flight at a time; while it runs, only the newest
    // position is kept. Prevents queue buildup when a cold cache makes the
    // worker slower than the tick rate.
    bool m_requestInFlight = false;
    bool m_hasPendingRequest = false;
    double m_pendingLat = 0;
    double m_pendingLon = 0;

    QElapsedTimer m_lastUpdate;

    static constexpr int FallbackUpdateIntervalMs =
        NavigationCadence::RenderTickMs * NavigationCadence::RoadInfoEveryTicks;
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
