#pragma once

#include "RouteModels.h"
#include <tuple>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace RouteHelpers {

// Project point onto line segment, return closest point clamped to segment.
// Uses a local equirectangular projection (longitude scaled by cos(lat))
// so the projection parameter is computed in an approximately isotropic
// metric frame. Without this scaling, segments at non-equatorial latitudes
// are distorted along the longitude axis and the projected point drifts
// toward the longitudinal endpoint.
inline LatLng closestPointOnSegment(const LatLng &point,
                                     const LatLng &segA,
                                     const LatLng &segB)
{
    constexpr double DegToRad = M_PI / 180.0;
    double cosLat = std::cos(point.latitude * DegToRad);

    double dx = (segB.longitude - segA.longitude) * cosLat;
    double dy = segB.latitude - segA.latitude;
    double lenSq = dx * dx + dy * dy;

    if (lenSq < 1e-18) return segA;

    double px = (point.longitude - segA.longitude) * cosLat;
    double py = point.latitude - segA.latitude;

    double t = std::clamp((px * dx + py * dy) / lenSq, 0.0, 1.0);

    // Convert projection parameter back to raw lat/lng
    return {segA.latitude + t * dy,
            segA.longitude + t * (dx / cosLat)};
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

// Parse Valhalla route response JSON
inline Route parseRouteResponse(const QByteArray &data)
{
    Route route;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "parseRouteResponse: JSON parse error:" << err.errorString();
        return route;
    }

    QJsonObject root = doc.object();
    QJsonObject trip = root[QStringLiteral("trip")].toObject();
    QJsonArray legs = trip[QStringLiteral("legs")].toArray();
    if (legs.isEmpty()) {
        qWarning() << "parseRouteResponse: no legs in JSON";
        return route;
    }

    QJsonObject leg = legs[0].toObject();
    QJsonObject summary = leg[QStringLiteral("summary")].toObject();

    // Decode shape (polyline, precision 6)
    QString shape = leg[QStringLiteral("shape")].toString();
    route.waypoints = decodePolyline(shape, 6);
    qDebug() << "parseRouteResponse: decoded waypoints:" << route.waypoints.size();

    // Total distance/duration
    route.distance = summary[QStringLiteral("length")].toDouble() * 1000.0; // km → m
    route.duration = summary[QStringLiteral("time")].toDouble();

    // Parse maneuvers
    QJsonArray maneuvers = leg[QStringLiteral("maneuvers")].toArray();
    qDebug() << "parseRouteResponse: parsing" << maneuvers.size() << "maneuvers";
    for (const auto &m : maneuvers) {
        QJsonObject obj = m.toObject();
        RouteInstruction instr;

        int typeCode = obj[QStringLiteral("type")].toInt();
        // Skip kStart/kStartRight/kStartLeft (1-3) and kDestination/*Right/*Left (4-6).
        // These bookend every Valhalla route as placeholders — not real turns. When
        // kept, kStart at originalShapeIndex=0 becomes the first "upcoming maneuver"
        // until currentSegment advances past the first real turn, showing a growing
        // "straight ahead" distance counter at trip start.
        if (typeCode >= 1 && typeCode <= 6)
            continue;
        instr.type = mapValhallaType(typeCode);
        instr.distance = obj[QStringLiteral("length")].toDouble() * 1000.0; // km → m
        instr.duration = obj[QStringLiteral("time")].toDouble();
        instr.originalShapeIndex = obj[QStringLiteral("begin_shape_index")].toInt();
        instr.instructionText = obj[QStringLiteral("instruction")].toString();

        // Street names
        QJsonArray streets = obj[QStringLiteral("street_names")].toArray();
        if (!streets.isEmpty()) {
            instr.streetName = streets[0].toString();
        } else {
            QJsonArray beginStreets = obj[QStringLiteral("begin_street_names")].toArray();
            if (!beginStreets.isEmpty())
                instr.streetName = beginStreets[0].toString();
        }

        // Verbal instructions
        instr.verbalAlertInstruction =
            obj[QStringLiteral("verbal_transition_alert_instruction")].toString();
        instr.verbalPreTransitionInstruction =
            obj[QStringLiteral("verbal_pre_transition_instruction")].toString();
        instr.verbalSuccinctInstruction =
            obj[QStringLiteral("verbal_succinct_transition_instruction")].toString();
        instr.verbalMultiCue = obj[QStringLiteral("verbal_multi_cue")].toBool();

        // Roundabout
        if (typeCode == 26) {
            instr.roundaboutExitCount =
                obj[QStringLiteral("roundabout_exit_count")].toInt();
        }
        instr.bearingBefore = obj[QStringLiteral("begin_heading")].toDouble();
        instr.bearingAfter = obj[QStringLiteral("end_heading")].toDouble();

        // Set location from route waypoints
        if (instr.originalShapeIndex >= 0 && instr.originalShapeIndex < route.waypoints.size()) {
            instr.location = route.waypoints[instr.originalShapeIndex];
        }

        route.instructions.append(instr);
    }

    return route;
}

} // namespace RouteHelpers
