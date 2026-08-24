#pragma once

#include "routing/RouteModels.h"

#include <algorithm>
#include <cmath>

class MapCameraPolicy
{
public:
    static double routeOverviewZoom(const QList<LatLng> &shape,
                                    double minZoom = 11.0,
                                    double maxZoom = 15.0)
    {
        if (shape.size() < 2)
            return maxZoom;
        double minLat = shape.first().latitude;
        double maxLat = minLat;
        double minLon = shape.first().longitude;
        double maxLon = minLon;
        for (const LatLng &point : shape) {
            minLat = std::min(minLat, point.latitude);
            maxLat = std::max(maxLat, point.latitude);
            minLon = std::min(minLon, point.longitude);
            maxLon = std::max(maxLon, point.longitude);
        }
        constexpr double EarthRadius = 6371000.0;
        const double centerLat = (minLat + maxLat) * 0.5 * M_PI / 180.0;
        const double height = (maxLat - minLat) * M_PI / 180.0 * EarthRadius;
        const double width = (maxLon - minLon) * M_PI / 180.0 * EarthRadius
            * std::max(0.01, std::cos(centerLat));
        // The rider remains anchored near the screen center during the brief
        // overview rather than moving to the route bbox center, so reserve
        // roughly twice the one-sided extent plus padding.
        const double extent = std::max(width, height) * 2.2;
        if (extent < 1.0)
            return maxZoom;

        // Web-Mercator ground resolution at z0 is 156543 m/px. Reserve a
        // conservative 240 px square for route context after map overlays.
        constexpr double GroundResolutionZ0 = 156543.03392;
        constexpr double ViewportPixels = 240.0;
        const double zoom = std::log2(
            GroundResolutionZ0 * std::max(0.01, std::cos(centerLat))
            * ViewportPixels / extent);
        return std::clamp(zoom, minZoom, maxZoom);
    }

    static double maneuverFocusDistance(double firstDistance,
                                        double secondDistance,
                                        double lookAheadMeters)
    {
        if (secondDistance > 0.0 && secondDistance < lookAheadMeters)
            return std::max(firstDistance, secondDistance);
        return firstDistance;
    }
};
