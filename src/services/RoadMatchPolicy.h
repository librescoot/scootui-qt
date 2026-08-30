#pragma once

#include <QList>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>

struct RoadMatchCandidateScore {
    QString key;
    double distanceMeters = std::numeric_limits<double>::max();
    double bearingDegrees = -1.0;
    bool tunnel = false;
    bool oneWay = false;
    bool oneWayReverse = false;
};

struct RoadMatchSelection {
    int index = -1;
    bool confident = false;
};

// Hold the last confident 1 Hz road match across isolated misses. The marker's
// distance hysteresis below remains the immediate safety release if the retained
// segment is no longer physically plausible.
class RoadMatchRetentionState
{
public:
    bool retainAfterMiss()
    {
        return ++m_consecutiveMisses < MissesBeforeClear;
    }

    void matched() { m_consecutiveMisses = 0; }
    void reset() { m_consecutiveMisses = 0; }
    int consecutiveMisses() const { return m_consecutiveMisses; }

    static constexpr int MissesBeforeClear = 3;

private:
    int m_consecutiveMisses = 0;
};

// Presentation hysteresis for the already-selected free-drive road segment.
// Candidate confidence is produced at 1 Hz by RoadMatchPolicy; this state only
// decides whether the 20 Hz marker presentation should use that segment.
class FreeDriveSnapState
{
public:
    bool update(bool confidentMatch, double distanceMeters)
    {
        const bool validDistance = std::isfinite(distanceMeters)
            && distanceMeters >= 0.0;
        if (m_locked) {
            if (!confidentMatch || !validDistance
                || distanceMeters > ReleaseMeters)
                m_locked = false;
        } else if (confidentMatch && validDistance
                   && distanceMeters <= AcquireMeters) {
            m_locked = true;
        }
        return m_locked;
    }

    void reset() { m_locked = false; }
    bool locked() const { return m_locked; }

    // Acquisition stays near the vehicle's typical ~19 m reported EPH: an
    // uncertain but merely nearby road may still supply metadata, but should
    // not pull a parked marker out of a courtyard. Once acquired, the wider
    // release threshold absorbs normal receiver drift.
    static constexpr double AcquireMeters = 20.0;
    static constexpr double ReleaseMeters = 30.0;

private:
    bool m_locked = false;
};

class RoadMatchPolicy
{
public:
    static RoadMatchSelection select(const QList<RoadMatchCandidateScore> &candidates,
                                     double headingDegrees, bool headingReliable,
                                     const QString &previousKey,
                                     double maxDistanceMeters)
    {
        struct Ranked { int index; double score; double angle; };
        QList<Ranked> ranked;
        ranked.reserve(candidates.size());

        for (int i = 0; i < candidates.size(); ++i) {
            const auto &candidate = candidates[i];
            if (!std::isfinite(candidate.distanceMeters)
                || candidate.distanceMeters > maxDistanceMeters)
                continue;

            const double angle = headingReliable
                ? travelAngleDifference(headingDegrees, candidate)
                : 0.0;
            double score = candidate.distanceMeters;
            if (headingReliable && candidate.bearingDegrees >= 0.0) {
                score += angle * 0.20;
                if (angle > 70.0)
                    score += 20.0;
            }
            if (!previousKey.isEmpty() && candidate.key == previousKey)
                score -= 4.0;
            if (candidate.tunnel && candidate.key != previousKey)
                score += 6.0;
            ranked.append({i, score, angle});
        }

        if (ranked.isEmpty())
            return {};
        std::sort(ranked.begin(), ranked.end(), [](const Ranked &a,
                                                   const Ranked &b) {
            return a.score < b.score;
        });

        const Ranked &best = ranked.first();
        RoadMatchSelection result;
        result.index = best.index; // useful for metadata even when not snap-safe

        // Keep an established road while it remains close and directionally
        // plausible. At a stop, course is intentionally unavailable: retaining
        // is safe, while acquiring or switching from position alone is not.
        if (!previousKey.isEmpty()) {
            for (const Ranked &rankedCandidate : ranked) {
                const auto &candidate = candidates[rankedCandidate.index];
                if (candidate.key == previousKey
                    && candidate.distanceMeters <= FreeDriveSnapState::ReleaseMeters
                    && (!headingReliable
                        || rankedCandidate.angle <= AlignmentDegrees)) {
                    result.index = rankedCandidate.index;
                    result.confident = true;
                    return result;
                }
            }
        }

        // New acquisitions and switches require actual direction of travel.
        // A close point alone is exactly what pulls the marker into courtyards
        // when GPS drifts near a driveway.
        const auto &candidate = candidates[best.index];
        if (!headingReliable || best.angle > AlignmentDegrees
            || candidate.distanceMeters > FreeDriveSnapState::AcquireMeters)
            return result;

        // With no directed/one-way information, two close parallel candidates
        // are indistinguishable (notably divided carriageways). Do not guess.
        for (int i = 1; i < ranked.size(); ++i) {
            const Ranked &other = ranked[i];
            const auto &otherCandidate = candidates[other.index];
            if (otherCandidate.distanceMeters <= FreeDriveSnapState::AcquireMeters
                && other.angle <= AlignmentDegrees
                && undirectedAngleDifference(candidate.bearingDegrees,
                                             otherCandidate.bearingDegrees)
                    <= ParallelDegrees) {
                return result;
            }
        }

        result.confident = true;
        return result;
    }

private:
    static constexpr double AlignmentDegrees = 35.0;
    static constexpr double ParallelDegrees = 15.0;

    static double travelAngleDifference(double headingDegrees,
                                        const RoadMatchCandidateScore &candidate)
    {
        double bearing = candidate.bearingDegrees;
        if (candidate.oneWayReverse && bearing >= 0.0)
            bearing = std::fmod(bearing + 180.0, 360.0);
        if (candidate.oneWay || candidate.oneWayReverse)
            return directedAngleDifference(headingDegrees, bearing);
        return undirectedAngleDifference(headingDegrees, bearing);
    }

    static double directedAngleDifference(double a, double b)
    {
        if (!std::isfinite(a) || !std::isfinite(b) || b < 0.0)
            return 180.0;
        double d = std::abs(std::fmod(a - b, 360.0));
        return d > 180.0 ? 360.0 - d : d;
    }

    static double undirectedAngleDifference(double a, double b)
    {
        if (!std::isfinite(a) || !std::isfinite(b) || b < 0.0)
            return 90.0;
        double d = std::abs(std::fmod(a - b, 360.0));
        if (d > 180.0) d = 360.0 - d;
        return std::min(d, 180.0 - d);
    }
};
