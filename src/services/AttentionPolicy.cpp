#include "AttentionPolicy.h"

#include <algorithm>

namespace {
int priority(const QVariantMap &entry) { return entry.value(QStringLiteral("priority"), 4).toInt(); }
bool valid(const QVariantMap &entry, qint64 nowMs)
{
    const qint64 until = entry.value(QStringLiteral("validUntil"), 0).toLongLong();
    return until == 0 || until > nowMs;
}
}

AttentionSelection AttentionPolicy::select(const QList<QVariantMap> &conditions,
                                            const QList<QVariantMap> &events,
                                            const QVariantMap &navigation,
                                            bool riding, qint64 nowMs)
{
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
    const bool imminent = hasNavigation
        && navigation.value(QStringLiteral("status"), 2).toInt() == 2
        && navigation.value(QStringLiteral("distance"), 0.0).toDouble() <= 120.0;
    if (hasNavigation) {
        QVariantMap nav = navigation;
        nav[QStringLiteral("priority")] = imminent ? 1 : 3;
        nav[QStringLiteral("kind")] = QStringLiteral("nav");
        candidates.append(nav);
    }

    for (const auto &event : events) {
        if (!valid(event, nowMs))
            continue;
        if (riding && priority(event) >= 4)
            continue;
        candidates.append(event);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const QVariantMap &a,
                                                               const QVariantMap &b) {
        if (priority(a) != priority(b))
            return priority(a) < priority(b);
        return a.value(QStringLiteral("order"), 0).toULongLong()
            < b.value(QStringLiteral("order"), 0).toULongLong();
    });

    AttentionSelection result;
    if (!candidates.isEmpty())
        result.main = candidates.first();

    if (!result.main.isEmpty() && result.main.value(QStringLiteral("kind")) != QLatin1String("nav")
        && hasNavigation) {
        result.companion = navigation;
        result.companion[QStringLiteral("kind")] = QStringLiteral("nav");
        result.companion[QStringLiteral("priority")] = imminent ? 1 : 3;
    }

    const bool expanded = !result.main.isEmpty()
        && (priority(result.main) == 0
            || (result.main.value(QStringLiteral("kind")) == QLatin1String("nav") && imminent));
    result.height = result.main.isEmpty() ? 0 : (expanded ? 96 : 56);
    if (!result.companion.isEmpty())
        result.height = priority(result.main) == 0 ? 128 : 92;
    result.criticalCount = criticalCount;
    return result;
}
