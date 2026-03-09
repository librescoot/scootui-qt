#pragma once

#include "RouteModels.h"
#include <tuple>
#include <algorithm>

namespace RouteHelpers {

// Project point onto line segment, return closest point clamped to segment
inline LatLng closestPointOnSegment(const LatLng &point,
                                     const LatLng &segA,
                                     const LatLng &segB)
{
    double dx = segB.longitude - segA.longitude;
    double dy = segB.latitude - segA.latitude;
    double lenSq = dx * dx + dy * dy;

    if (lenSq < 1e-15) return segA;

    double t = ((point.longitude - segA.longitude) * dx +
                (point.latitude - segA.latitude) * dy) / lenSq;
    t = std::clamp(t, 0.0, 1.0);

    return {segA.latitude + t * dy, segA.longitude + t * dx};
}

// Find closest point on entire route polyline
// Returns: (closest point, segment index, distance in meters)
inline std::tuple<LatLng, int, double> findClosestPointOnRoute(
    const LatLng &position, const QList<LatLng> &waypoints)
{
    if (waypoints.size() < 2)
        return {position, 0, 999999.0};

    LatLng bestPoint = waypoints.first();
    int bestIndex = 0;
    double bestDist = position.distanceTo(bestPoint);

    for (int i = 0; i < waypoints.size() - 1; ++i) {
        LatLng proj = closestPointOnSegment(position, waypoints[i], waypoints[i + 1]);
        double dist = position.distanceTo(proj);
        if (dist < bestDist) {
            bestDist = dist;
            bestPoint = proj;
            bestIndex = i;
        }
    }

    return {bestPoint, bestIndex, bestDist};
}

// Find upcoming instructions from current segment position
// Returns up to maxInstructions instructions ahead
inline QList<RouteInstruction> findUpcomingInstructions(
    const LatLng &currentPosition,
    const Route &route,
    int currentSegmentIndex,
    int maxInstructions = 3)
{
    QList<RouteInstruction> result;

    for (const auto &instr : route.instructions) {
        if (instr.originalShapeIndex >= currentSegmentIndex) {
            RouteInstruction adjusted = instr;
            // Recalculate distance from current position to instruction start
            adjusted.distance = currentPosition.distanceTo(instr.location);
            result.append(adjusted);
            if (result.size() >= maxInstructions) break;
        }
    }

    return result;
}

// Calculate remaining distance from current position to destination along route
inline double remainingDistanceAlongRoute(const LatLng &position,
                                           const QList<LatLng> &waypoints,
                                           int segmentIndex)
{
    if (waypoints.isEmpty() || segmentIndex >= waypoints.size() - 1)
        return 0;

    // Distance from current position to next waypoint
    double dist = position.distanceTo(waypoints[segmentIndex + 1]);

    // Add distances of remaining segments
    for (int i = segmentIndex + 1; i < waypoints.size() - 1; ++i) {
        dist += waypoints[i].distanceTo(waypoints[i + 1]);
    }

    return dist;
}

} // namespace RouteHelpers
