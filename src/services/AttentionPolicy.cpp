#include "AttentionPolicy.h"

#include <algorithm>

namespace {
int priority(const QVariantMap &entry)
{
    // Keep the public 0/2/3/4 priorities; navigation sits between warning and success.
    if (entry.value(QStringLiteral("kind")) == QLatin1String("nav"))
        return 3;
    const int value = entry.value(QStringLiteral("priority"), 4).toInt();
    return value >= 3 ? value + 1 : value;
}

QVariantMap selectPeer(const QList<QVariantMap> &candidates, qint64 nowMs,
                       AttentionCycleState *cycle)
{
    if (candidates.isEmpty()) {
        if (cycle)
            *cycle = {};
        return {};
    }
    QVariantMap selected = candidates.first();
    if (!cycle)
        return selected;
    const int topPriority = priority(selected);
    QList<QVariantMap> peers;
    for (const auto &candidate : candidates) {
        if (priority(candidate) == topPriority)
            peers.append(candidate);
    }
    if (cycle->priority == topPriority) {
        const auto current = std::find_if(peers.cbegin(), peers.cend(), [cycle](const QVariantMap &entry) {
            return entry.value(QStringLiteral("order")).toULongLong() == cycle->order;
        });
        if (current != peers.cend() && nowMs - cycle->presentedAtMs < AttentionPolicy::DwellMs) {
            selected = *current;
        } else {
            const auto next = std::find_if(peers.cbegin(), peers.cend(), [cycle](const QVariantMap &entry) {
                return entry.value(QStringLiteral("order")).toULongLong() > cycle->order;
            });
            selected = next == peers.cend() ? peers.first() : *next;
        }
    }
    const quint64 order = selected.value(QStringLiteral("order")).toULongLong();
    if (cycle->priority != topPriority || cycle->order != order) {
        cycle->order = order;
        cycle->priority = topPriority;
        cycle->presentedAtMs = nowMs;
    }
    return selected;
}

bool valid(const QVariantMap &entry, qint64 nowMs)
{
    const qint64 until = entry.value(QStringLiteral("validUntil"), 0).toLongLong();
    return until == 0 || until > nowMs;
}
}

AttentionSelection AttentionPolicy::select(const QList<QVariantMap> &conditions,
                                            const QList<QVariantMap> &events,
                                            const QVariantMap &navigation,
                                            bool riding, qint64 nowMs,
                                            AttentionCycleState *cycle,
                                            AttentionCycleState *companionCycle)
{
    Q_UNUSED(riding);
    QList<QVariantMap> candidates;
    int criticalCount = 0;
    for (const auto &condition : conditions) {
        if (condition.value(QStringLiteral("presentationOnly"), false).toBool())
            continue;
        if ((condition.value(QStringLiteral("surface")).toString() == QLatin1String("map")
             || condition.value(QStringLiteral("source")).toString() == QLatin1String("map"))
            && !condition.value(QStringLiteral("surfaceEligible"), true).toBool())
            continue;
        candidates.append(condition);
        if (priority(condition) == 0)
            ++criticalCount;
    }

    const bool hasNavigation = !navigation.isEmpty()
        && navigation.value(QStringLiteral("valid"), false).toBool()
        && navigation.value(QStringLiteral("status"), 0).toInt() != 5;
    if (hasNavigation) {
        QVariantMap nav = navigation;
        nav[QStringLiteral("priority")] = 3;
        nav[QStringLiteral("kind")] = QStringLiteral("nav");
        candidates.append(nav);
    }

    for (const auto &event : events) {
        if (!valid(event, nowMs))
            continue;
        candidates.append(event);
        if (priority(event) == 0)
            ++criticalCount;
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const QVariantMap &a,
                                                               const QVariantMap &b) {
        if (priority(a) != priority(b))
            return priority(a) < priority(b);
        return a.value(QStringLiteral("order"), 0).toULongLong()
            < b.value(QStringLiteral("order"), 0).toULongLong();
    });

    AttentionSelection result;
    result.main = selectPeer(candidates, nowMs, cycle);
    QList<QVariantMap> companions;
    if (result.main.value(QStringLiteral("kind")) == QLatin1String("nav")) {
        for (const auto &candidate : candidates) {
            if (candidate.value(QStringLiteral("kind")) != QLatin1String("nav"))
                companions.append(candidate);
        }
    } else if (hasNavigation) {
        const int status = navigation.value(QStringLiteral("status"), 2).toInt();
        if (status == 2 || status == 4) {
            QVariantMap nav = navigation;
            nav[QStringLiteral("kind")] = QStringLiteral("nav");
            companions.append(nav);
        }
    }
    result.companion = selectPeer(companions, nowMs, companionCycle);
    for (const auto &candidate : candidates) {
        if (candidate == result.main || candidate == result.companion
            || candidate.value(QStringLiteral("kind")) == QLatin1String("nav"))
            continue;
        const int value = candidate.value(QStringLiteral("priority"), 4).toInt();
        const QString severity = value == 0 ? QStringLiteral("error")
            : value <= 2 ? QStringLiteral("warning")
            : value == 3 ? QStringLiteral("success")
            : value == 5 ? QStringLiteral("debug") : QStringLiteral("info");
        result.queuedCounts[severity] = result.queuedCounts.value(severity, 0).toInt() + 1;
    }
    result.criticalCount = criticalCount;
    return result;
}
