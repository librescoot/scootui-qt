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
};

struct RoadMatchSelection {
    int index = -1;
    bool confident = false;
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
                ? undirectedAngleDifference(headingDegrees,
                                            candidate.bearingDegrees)
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
        const auto &candidate = candidates[best.index];
        const double margin = ranked.size() > 1
            ? ranked[1].score - best.score
            : std::numeric_limits<double>::infinity();
        const bool priorStable = !previousKey.isEmpty()
            && candidate.key == previousKey;
        const bool directionFits = headingReliable && best.angle <= 35.0;

        RoadMatchSelection result;
        result.index = best.index;
        result.confident = candidate.distanceMeters <= 6.0
            || margin >= 3.0 || priorStable || directionFits;
        return result;
    }

private:
    static double undirectedAngleDifference(double a, double b)
    {
        if (!std::isfinite(a) || !std::isfinite(b) || b < 0.0)
            return 90.0;
        double d = std::abs(std::fmod(a - b, 360.0));
        if (d > 180.0) d = 360.0 - d;
        return std::min(d, 180.0 - d);
    }
};
