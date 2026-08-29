#pragma once

#include <algorithm>
#include <cmath>

// Reconciles speed-integrated travel against a coarse odometer without
// treating the current quantized bucket as an exact continuous position.
// Odometer changes are observations at bucket crossings; their residual is
// paid down gradually and can never dominate the speed feed-forward.
class OdometerReconciler
{
public:
    double advance(double odometerMeters, double speedMs, double dtSeconds)
    {
        if (!std::isfinite(odometerMeters) || !std::isfinite(speedMs)
            || !std::isfinite(dtSeconds) || dtSeconds <= 0.0)
            return 0.0;

        speedMs = std::max(0.0, speedMs);
        const double feedForward = speedMs * dtSeconds;

        if (!m_seeded) {
            if (odometerMeters > 0.0) {
                m_seeded = true;
                m_lastOdometer = odometerMeters;
                m_sinceEdge = 0.0;
                m_haveCompleteInterval = false;
            }
        } else {
            const double edgeDelta = odometerMeters - m_lastOdometer;
            if (edgeDelta < -1.0 || edgeDelta > MaxPlausibleEdgeMeters) {
                reset(odometerMeters);
            } else if (edgeDelta >= MinEdgeMeters) {
                // The seed can occur anywhere inside a quantized bucket, so
                // the first observed edge only establishes a true boundary.
                // Every later edge spans a complete measurable interval.
                if (m_haveCompleteInterval) {
                    m_pendingResidual += edgeDelta - m_sinceEdge;
                    m_pendingResidual = std::clamp(m_pendingResidual,
                                                   -MaxResidualMeters,
                                                   MaxResidualMeters);
                }
                m_haveCompleteInterval = true;
                m_lastOdometer = odometerMeters;
                m_sinceEdge = 0.0;
            }
        }

        // No movement means no reconciliation movement either. Once moving,
        // close at most 20% of this tick's feed-forward so an odometer edge
        // cannot create a visible speed surge or stop.
        double correction = 0.0;
        if (feedForward > 0.0 && std::abs(m_pendingResidual) > 1e-6) {
            const double limit = feedForward * MaxCorrectionFraction;
            correction = std::clamp(m_pendingResidual * ResidualRate * dtSeconds,
                                    -limit, limit);
            m_pendingResidual -= correction;
        }

        const double travelled = std::max(0.0, feedForward + correction);
        if (m_seeded)
            m_sinceEdge += travelled;
        return travelled;
    }

    void clear()
    {
        m_seeded = false;
        m_lastOdometer = 0.0;
        m_sinceEdge = 0.0;
        m_pendingResidual = 0.0;
        m_haveCompleteInterval = false;
    }

    double pendingResidual() const { return m_pendingResidual; }

private:
    void reset(double odometerMeters)
    {
        m_lastOdometer = std::max(0.0, odometerMeters);
        m_sinceEdge = 0.0;
        m_pendingResidual = 0.0;
        m_seeded = odometerMeters > 0.0;
        m_haveCompleteInterval = false;
    }

    static constexpr double MinEdgeMeters = 1.0;
    static constexpr double MaxPlausibleEdgeMeters = 1000.0;
    static constexpr double MaxResidualMeters = 100.0;
    static constexpr double MaxCorrectionFraction = 0.20;
    static constexpr double ResidualRate = 0.20; // closes 20%/second

    bool m_seeded = false;
    bool m_haveCompleteInterval = false;
    double m_lastOdometer = 0.0;
    double m_sinceEdge = 0.0;
    double m_pendingResidual = 0.0;
};

// Sticky route-presentation decision driven by cross-track distance from the
// unconstrained physical estimate and, while moving, its direction relative to
// the matched route leg. GPS horizontal error on the vehicle is commonly
// 15-30 m even under open sky, so fixed single-digit thresholds make normal
// receiver uncertainty look like a deliberate departure.
class RouteSnapState
{
public:
    bool update(double physicalDistanceMeters, int elapsedMs,
                double ephMeters = 0.0,
                double headingDifferenceDegrees = 0.0,
                bool headingReliable = false)
    {
        elapsedMs = std::max(0, elapsedMs);
        const double breakAway = breakAwayMeters(ephMeters);
        const double relock = relockMeters(ephMeters);
        const double headingDifference = std::abs(headingDifferenceDegrees);

        if (m_locked) {
            if (physicalDistanceMeters > breakAway)
                m_distanceDepartureMs += elapsedMs;
            else
                m_distanceDepartureMs = 0;

            // A rider continuing straight past a requested turn should become
            // visible before pure cross-track distance reaches the accuracy-
            // adjusted threshold. Require some real displacement as well as a
            // sustained direction mismatch so stale course at the turn itself
            // cannot release the snap.
            const bool directionalDeparture = hasDirectionalDepartureEvidence(
                physicalDistanceMeters, headingDifference, headingReliable);
            if (directionalDeparture)
                m_directionDepartureMs += elapsedMs;
            else
                m_directionDepartureMs = 0;

            if (m_distanceDepartureMs >= BreakAwayDwellMs
                || m_directionDepartureMs >= DirectionDepartureDwellMs) {
                m_locked = false;
                clearTimers();
            }
        } else {
            const bool directionAllowsRelock = !headingReliable
                || headingDifference <= RelockDirectionDegrees;
            if (physicalDistanceMeters < relock && directionAllowsRelock)
                m_relockMs += elapsedMs;
            else
                m_relockMs = 0;

            if (m_relockMs >= RelockDwellMs) {
                m_locked = true;
                clearTimers();
            }
        }
        return m_locked;
    }

    void reset(bool locked = true)
    {
        m_locked = locked;
        clearTimers();
    }

    bool locked() const { return m_locked; }

    static double breakAwayMeters(double ephMeters)
    {
        const double eph = std::isfinite(ephMeters) && ephMeters > 0.0
            ? ephMeters : DefaultEphMeters;
        return std::clamp(eph * BreakAwayEphFactor,
                          BreakAwayMinMeters, BreakAwayMaxMeters);
    }

    static double relockMeters(double ephMeters)
    {
        const double eph = std::isfinite(ephMeters) && ephMeters > 0.0
            ? ephMeters : DefaultEphMeters;
        return std::clamp(eph * RelockEphFactor,
                          RelockMinMeters, RelockMaxMeters);
    }

    static bool hasDirectionalDepartureEvidence(
        double physicalDistanceMeters, double headingDifferenceDegrees,
        bool headingReliable)
    {
        return headingReliable
            && std::abs(headingDifferenceDegrees) >= DirectionDepartureDegrees
            && physicalDistanceMeters > DirectionDepartureMinMeters;
    }

    static constexpr int BreakAwayDwellMs = 1500;
    static constexpr int RelockDwellMs = 2000;
    static constexpr double DirectionDepartureDegrees = 45.0;
    static constexpr double DirectionDepartureMinMeters = 15.0;
    static constexpr int DirectionDepartureDwellMs = 2000;
    static constexpr double RelockDirectionDegrees = 30.0;

private:
    void clearTimers()
    {
        m_distanceDepartureMs = 0;
        m_directionDepartureMs = 0;
        m_relockMs = 0;
    }

    static constexpr double DefaultEphMeters = 20.0;
    static constexpr double BreakAwayEphFactor = 1.25;
    static constexpr double BreakAwayMinMeters = 30.0;
    static constexpr double BreakAwayMaxMeters = 45.0;
    static constexpr double RelockEphFactor = 1.0;
    static constexpr double RelockMinMeters = 18.0;
    static constexpr double RelockMaxMeters = 30.0;

    bool m_locked = true;
    int m_distanceDepartureMs = 0;
    int m_directionDepartureMs = 0;
    int m_relockMs = 0;
};

class RoutePresentationPolicy
{
public:
    static bool allowsPolylineWalk(double speedKmh)
    {
        return std::isfinite(speedKmh)
            && speedKmh / 3.6 >= StationarySpeedMetersPerSecond;
    }

    static constexpr double StationarySpeedMetersPerSecond = 0.3;
};
