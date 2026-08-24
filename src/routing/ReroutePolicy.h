#pragma once

#include "routing/RouteModels.h"
#include "stores/GpsStore.h"

#include <algorithm>
#include <cmath>

class RerouteOriginSelector
{
public:
    struct Input {
        GpsSample gps;
        qint64 gpsAgeMs = -1;
        LatLng physicalEstimate;
        double physicalUncertaintyMeters = 1000.0;
    };

    static RouteOrigin select(const Input &input)
    {
        if (input.gps.hasValidCoordinate() && input.gps.hasFix()
            && input.gpsAgeMs >= 0 && input.gpsAgeMs <= MaxGpsAgeMs
            && input.gps.hasAcceptableAccuracy(MaxGpsEphMeters)) {
            RouteOrigin result;
            result.position = {input.gps.latitude, input.gps.longitude};
            result.heading = normalizedHeading(input.gps.course);
            result.headingToleranceDegrees = 40;

            // Advance only by the known receive/receiver age and cap it. This
            // avoids both routing from an old point and projecting stale data
            // far enough to cross onto a parallel road.
            if (input.gps.speedKmh >= MinCourseSpeedKmh) {
                const double projectionMs = std::min<double>(
                    input.gpsAgeMs + ReceiverBufferMs, MaxProjectionMs);
                result.position = project(result.position, result.heading,
                    input.gps.speedKmh / 3.6 * projectionMs / 1000.0);
            }

            const double eph = input.gps.ephMeters > 0.0
                ? input.gps.ephMeters : DefaultGpsEphMeters;
            result.radiusMeters = static_cast<int>(std::lround(std::clamp(
                eph * 1.5 + RouterMarginMeters,
                static_cast<double>(MinRouterRadiusMeters),
                static_cast<double>(MaxRouterRadiusMeters))));
            return result;
        }

        if (input.physicalEstimate.isValid()
            && std::isfinite(input.physicalUncertaintyMeters)
            && input.physicalUncertaintyMeters <= MaxEstimatorUncertaintyMeters) {
            RouteOrigin result;
            result.position = input.physicalEstimate;
            result.radiusMeters = static_cast<int>(std::lround(std::clamp(
                input.physicalUncertaintyMeters + RouterMarginMeters,
                static_cast<double>(MinRouterRadiusMeters),
                static_cast<double>(MaxEstimatorRouterRadiusMeters))));
            return result;
        }

        return {};
    }

private:
    static LatLng project(const LatLng &from, double headingDegrees,
                          double distanceMeters)
    {
        constexpr double EarthRadius = 6371000.0;
        constexpr double DegToRad = M_PI / 180.0;
        constexpr double RadToDeg = 180.0 / M_PI;
        if (distanceMeters <= 0.0 || headingDegrees < 0.0)
            return from;

        const double angular = distanceMeters / EarthRadius;
        const double bearing = headingDegrees * DegToRad;
        const double lat1 = from.latitude * DegToRad;
        const double lon1 = from.longitude * DegToRad;
        const double lat2 = std::asin(std::sin(lat1) * std::cos(angular)
            + std::cos(lat1) * std::sin(angular) * std::cos(bearing));
        const double lon2 = lon1 + std::atan2(
            std::sin(bearing) * std::sin(angular) * std::cos(lat1),
            std::cos(angular) - std::sin(lat1) * std::sin(lat2));
        return {lat2 * RadToDeg, lon2 * RadToDeg};
    }

    static double normalizedHeading(double heading)
    {
        if (!std::isfinite(heading))
            return -1.0;
        heading = std::fmod(heading, 360.0);
        return heading < 0.0 ? heading + 360.0 : heading;
    }

    static constexpr qint64 MaxGpsAgeMs = 2500;
    static constexpr double MaxGpsEphMeters = 35.0;
    static constexpr double MaxEstimatorUncertaintyMeters = 40.0;
    static constexpr double ReceiverBufferMs = 300.0;
    static constexpr double MaxProjectionMs = 2000.0;
    static constexpr double MinCourseSpeedKmh = 3.0;
    static constexpr double DefaultGpsEphMeters = 15.0;
    static constexpr double RouterMarginMeters = 8.0;
    static constexpr int MinRouterRadiusMeters = 15;
    static constexpr int MaxRouterRadiusMeters = 60;
    static constexpr int MaxEstimatorRouterRadiusMeters = 50;
};

class RerouteEpisodeGate
{
public:
    bool shouldRequest(bool offRoute, bool originAvailable)
    {
        if (!offRoute) {
            m_requested = false;
            return false;
        }
        if (!originAvailable || m_requested)
            return false;
        m_requested = true;
        return true;
    }

    void retryReady() { m_requested = false; }
    void reset() { m_requested = false; }
    bool hasRequested() const { return m_requested; }

private:
    bool m_requested = false;
};
