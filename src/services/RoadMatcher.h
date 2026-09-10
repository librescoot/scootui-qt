#pragma once

#include "VectorTileDecoder.h"
#include "RoadMatchPolicy.h"
#include <QHash>

struct RoadMatchCandidate {
    RoadMatchCandidateScore policy;
    QString name;
    QString refs;
    QString kind;
    QString routeNetworks;
    QString maxspeed;
    double lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    double snappedLat = 0, snappedLon = 0;
    double actualDistanceMeters = std::numeric_limits<double>::max();
};

// Only value-owned, implicitly shared tile data crosses the thread boundary.
// No database handles, stores, services or mutable cache references are allowed.
struct RoadMatchRequest {
    quint64 sequence = 0;
    quint64 routeGeneration = 0;
    int mapGeneration = 0;
    qint64 submittedMs = 0;
    double lat = 0, lon = 0;
    double heading = 0;
    bool headingReliable = false;
    double maxDistance = 35;
    QString routeName;
    QString previousKey;
    QHash<quint64, VectorTile::Tile> tiles;
    bool waitingForTiles = false;

    bool isCurrent(quint64 latestSequence, quint64 latestRouteGeneration,
                   int latestMapGeneration, bool hasPosition) const
    {
        return hasPosition && sequence == latestSequence
            && routeGeneration == latestRouteGeneration
            && mapGeneration == latestMapGeneration;
    }
};

struct RoadMatchResult {
    RoadMatchCandidate chosen;
    RoadMatchSelection selection;
    bool waitingForTiles = false;
};

RoadMatchResult matchRoad(const RoadMatchRequest &request);
