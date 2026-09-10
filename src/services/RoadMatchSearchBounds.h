#pragma once

#include <QPointF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

// Conservative tile-space bounds for the matcher (40 m acceptance plus
// a possible 12 m route-name preference). Filter before inverse-Mercator
// conversion and scoring; long segments crossing the window must survive.
struct RoadMatchSearchBounds {
    double minX, minY, maxX, maxY;

    static RoadMatchSearchBounds around(double lat, double lon,
                                       int tileX, int tileY, int zoom,
                                       double extent)
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double radius = 64.0;
        constexpr double earthRadius = 6371000.0;
        const double dLat = radius / earthRadius * 180.0 / pi;
        const double dLon = dLat / std::max(0.01, std::cos(lat * pi / 180.0));
        const double n = double(1 << zoom);
        const auto x = [=](double longitude) {
            return ((longitude + 180.0) / 360.0 * n - tileX) * extent;
        };
        const auto y = [=](double latitude) {
            latitude = std::clamp(latitude, -85.05112878, 85.05112878);
            const double r = latitude * pi / 180.0;
            const double slippy = (1.0 - std::asinh(std::tan(r)) / pi) * 0.5 * n;
            return (slippy - (n - 1.0 - tileY)) * extent;
        };
        return {x(lon - dLon), y(lat + dLat), x(lon + dLon), y(lat - dLat)};
    }

    bool intersects(const QVector<QVector<QPointF>> &parts) const
    {
        for (const auto &points : parts) {
            if (points.size() < 2)
                continue;
            double left = std::numeric_limits<double>::infinity();
            double top = left, right = -left, bottom = -left;
            for (const auto &p : points) {
                left = std::min(left, p.x()); right = std::max(right, p.x());
                top = std::min(top, p.y()); bottom = std::max(bottom, p.y());
            }
            if (right >= minX && left <= maxX && bottom >= minY && top <= maxY)
                return true;
        }
        return false;
    }
};
