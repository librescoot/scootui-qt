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

// Find upcoming instructions from current segment position.
// `snappedPos` is expected to be the rider's position projected onto the
// current segment; `currentSegmentIndex` indexes into `route.waypoints`
// (segment S runs from waypoints[S] to waypoints[S+1]).
//
// Distances are computed ALONG the route shape — remainder of the current
// segment plus the length of each intervening segment up to the maneuver's
// originalShapeIndex. Great-circle distance to the maneuver point
// understates long curvy approaches by 30-50 %; along-route matches what
// the voice line actually means by "300 m to turn".
//
// `hideStart` skips any maneuver flagged `isStart` (Valhalla kStart-family).
// Those sit at shape[0]; once the rider has driven away, they stop being
// useful as a TBT banner — the caller flips this true after the tracker
// advances past segment 0 or a short elapsed timer expires.
inline QList<RouteInstruction> findUpcomingInstructions(
    const LatLng &snappedPos,
    const Route &route,
    int currentSegmentIndex,
    int maxInstructions = 3,
    bool hideStart = false)
{
    QList<RouteInstruction> result;
    const auto &shape = route.waypoints;

    for (const auto &instr : route.instructions) {
        // Strictly-behind filter: a maneuver at a shape index lower than the
        // current segment start has been passed. We allow equality ("at the
        // current segment") so kStart can appear as the first upcoming at
        // trip start (currentSegmentIndex=0, originalShapeIndex=0) and
        // kDestination can appear as "Arrive now" in the final moment.
        if (instr.originalShapeIndex < currentSegmentIndex)
            continue;

        if (hideStart && instr.isStart)
            continue;

        RouteInstruction adjusted = instr;

        double along = 0;
        if (instr.originalShapeIndex == currentSegmentIndex) {
            // Maneuver is AT the current segment start — we're on top of it.
            // Report 0 so the UI can render "now" rather than a stale
            // remainder of the previous segment.
            along = 0;
        } else if (currentSegmentIndex >= 0 &&
                   currentSegmentIndex + 1 < shape.size()) {
            along = snappedPos.distanceTo(shape[currentSegmentIndex + 1]);
            int lastShapeIdx = std::min(instr.originalShapeIndex,
                                         static_cast<int>(shape.size()) - 1);
            for (int i = currentSegmentIndex + 1; i < lastShapeIdx; ++i)
                along += shape[i].distanceTo(shape[i + 1]);
        } else {
            // Fallback: no valid segment state — use great-circle to keep
            // something sane in the UI rather than emit 0.
            along = snappedPos.distanceTo(instr.location);
        }
        adjusted.distance = along;

        result.append(adjusted);
        if (result.size() >= maxInstructions) break;
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
        instr.type = mapValhallaType(typeCode);
        // kStart/kStartRight/kStartLeft flag — lets the UI hide the distance
        // counter (it would just say 0 m) and lets the caller drop this
        // maneuver a few seconds into the trip.
        instr.isStart = (typeCode >= 1 && typeCode <= 3);
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
        instr.verbalPostTransitionInstruction =
            obj[QStringLiteral("verbal_post_transition_instruction")].toString();
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

// Parse Valhalla /trace_attributes response into per-shape-segment EdgeAttrs.
// Walks the `edges` array, fanning each edge's attributes across the shape
// range [begin_shape_index, end_shape_index). The returned list is sized to
// `segmentCount` (== waypoints.size() - 1) so a single index lookup gives the
// edge for any shape segment; uncovered slots stay default-constructed.
//
// Returns empty list on parse error or if `edges` is missing — caller treats
// that as "trace_attributes had no useful data, fall through to tile path".
inline QList<EdgeAttrs> parseTraceAttributesResponse(const QByteArray &data,
                                                       int segmentCount)
{
    QList<EdgeAttrs> result;
    if (segmentCount <= 0)
        return result;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "parseTraceAttributesResponse: JSON parse error:" << err.errorString();
        return result;
    }

    QJsonArray edges = doc.object().value(QStringLiteral("edges")).toArray();
    if (edges.isEmpty())
        return result;

    result.resize(segmentCount);

    for (const auto &eVal : edges) {
        QJsonObject e = eVal.toObject();
        int begin = e.value(QStringLiteral("begin_shape_index")).toInt(-1);
        int end = e.value(QStringLiteral("end_shape_index")).toInt(-1);
        if (begin < 0 || end <= begin)
            continue;
        // end_shape_index points one past the last shape vertex of the edge,
        // so the segment range is [begin, end) — segment N covers shape N..N+1.
        int segHi = std::min(end, segmentCount);
        if (begin >= segHi)
            continue;

        EdgeAttrs attrs;
        QJsonArray names = e.value(QStringLiteral("names")).toArray();
        attrs.names.reserve(names.size());
        for (const auto &n : names)
            attrs.names.append(n.toString());
        attrs.roadClass = e.value(QStringLiteral("road_class")).toString();
        attrs.speedLimitKph = e.value(QStringLiteral("speed_limit")).toInt(0);
        attrs.tunnel = e.value(QStringLiteral("tunnel")).toBool();
        attrs.bridge = e.value(QStringLiteral("bridge")).toBool();

        for (int i = begin; i < segHi; ++i)
            result[i] = attrs;
    }

    return result;
}

} // namespace RouteHelpers
