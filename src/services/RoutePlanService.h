#pragma once

#include <QObject>
#include <QString>
#include "routing/RouteModels.h"

class MdbRepository;

// Persistence and wire format for a multi-hop plan.
//
// Persisted under dashboard.route-plan.<n>.* in the settings hash, which
// settings-service writes to /data/settings.toml; Redis is volatile. The
// navigation hash carries the wire copy: "waypoints" (JSON) and "current-step".
//
// Stop ids are session-local: assigned 1..N on load/parse and never persisted.
class RoutePlanService
{
public:
    explicit RoutePlanService(MdbRepository *repo);

    static constexpr int MaxStops = 32;

    RoutePlan load();
    bool save(const RoutePlan &plan, bool active = true);
    bool clear();

    // Whether the last load() was marked active; a finished trip must not resume.
    bool loadedActive() const { return m_loadedActive; }

    // JSON list of {"lat","lon","label"} for the navigation hash.
    static QString serializeWaypoints(const QList<RouteStop> &stops);
    // ok is false for empty or malformed input. Valid stops get ids 1..N.
    static QList<RouteStop> parseWaypoints(const QString &json, bool *ok = nullptr);

    static void assignIds(QList<RouteStop> &stops);

private:
    QString fieldKey(int index, const QString &field) const;
    void removeRecord(int index);

    MdbRepository *m_repo;
    int m_savedCount = 0;
    bool m_loadedActive = false;
};
