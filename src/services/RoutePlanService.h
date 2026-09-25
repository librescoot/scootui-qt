#pragma once

#include <QString>
#include "routing/RouteModels.h"

class RoutePlanService
{
public:
    static constexpr int MaxStops = 32;

    // Compatibility JSON list of {"lat","lon","label"}.
    static QString serializeWaypoints(const QList<RouteStop> &stops);
    // ok is false for empty or malformed input. Valid stops get ids 1..N.
    static QList<RouteStop> parseWaypoints(const QString &json, bool *ok = nullptr);

    static void assignIds(QList<RouteStop> &stops);
};
