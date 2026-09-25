#pragma once

#include "RouteHelpers.h"

namespace BlockedRoadSelector {

inline LatLng ahead(const Route &route, const LatLng &position, int currentSegment)
{
    if (!position.isValid() || currentSegment < 0
        || currentSegment + 1 >= route.waypoints.size())
        return {};

    const auto &shape = route.waypoints;
    const LatLng start = RouteHelpers::closestPointOnSegment(
        position, shape[currentSegment], shape[currentSegment + 1]);
    if (position.distanceTo(start) > 35.0)
        return {};

    double remaining = 60.0;
    LatLng from = start;
    for (int i = currentSegment; i + 1 < shape.size(); ++i) {
        const LatLng to = shape[i + 1];
        const double length = from.distanceTo(to);
        if (length >= remaining && length > 0.0) {
            const double t = remaining / length;
            return {from.latitude + t * (to.latitude - from.latitude),
                    from.longitude + t * (to.longitude - from.longitude)};
        }
        remaining -= length;
        from = to;
    }
    return {};
}

} // namespace BlockedRoadSelector
