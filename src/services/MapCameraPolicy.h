#pragma once

#include "routing/RouteModels.h"

#include <algorithm>
#include <cmath>

class MapCameraPolicy
{
public:
    static LatLng routeOverviewCenter(const QList<LatLng> &shape)
    {
        if (shape.isEmpty())
            return {};
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
        return {(minLat + maxLat) * 0.5, (minLon + maxLon) * 0.5};
    }

    // Defaults match MapService's overview bounds.
    static double routeOverviewZoom(const QList<LatLng> &shape,
                                    double minZoom = 9.0,
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
        const double extent = std::max(width, height) * 1.1;
        if (extent < 1.0)
            return maxZoom;

        // Keep markers clear of the destination strip above the map and the
        // road-name overlay at its bottom edge (map viewport is ~376 px tall).
        constexpr double GroundResolutionZ0 = 156543.03392;
        constexpr double ViewportPixels = 280.0;
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
