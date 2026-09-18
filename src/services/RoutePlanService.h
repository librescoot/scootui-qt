#pragma once

#include <QObject>
#include <QString>
#include "routing/RouteModels.h"

class MdbRepository;

// Persistence and wire format for a multi-hop plan.
//
// Durable copy: the settings hash, under dashboard.route-plan.<n>.*, so
// settings-service writes it to /data/settings.toml. Redis alone is volatile.
// Wire copy: the navigation hash, where "waypoints" is the JSON list external
// channels (cloud, BLE, CLI) push and "current-step" is the live pointer.
//
// Stop ids are a session-local handle only. They are assigned in list order by
// load() and parseWaypoints() and are never persisted, so the persisted order
// and current-step index are the source of truth across restarts.
class RoutePlanService
{
public:
    explicit RoutePlanService(MdbRepository *repo);

    // Upper bound on persisted stops. The settings-service schema uses an
    // indexed pattern, and a plan longer than this is not a scooter trip.
    static constexpr int MaxStops = 32;

    RoutePlan load();
    bool save(const RoutePlan &plan, bool active = true);
    bool clear();

    // Whether the plan returned by the last load() was marked active. A stale
    // list from a finished trip must not be resumed on boot.
    bool loadedActive() const { return m_loadedActive; }

    // JSON list of {"lat","lon","label"} for the navigation hash.
    static QString serializeWaypoints(const QList<RouteStop> &stops);
    // Parse a waypoints payload. ok is false for empty or malformed input, in
    // which case the returned list is empty. Valid stops get ids 1..N.
    static QList<RouteStop> parseWaypoints(const QString &json, bool *ok = nullptr);

    // Assign 1..N ids in list order. Used after a load or parse, and after a
    // mutation that introduced new stops.
    static void assignIds(QList<RouteStop> &stops);

private:
    QString fieldKey(int index, const QString &field) const;
    void removeRecord(int index);

    MdbRepository *m_repo;
    int m_savedCount = 0;
    bool m_loadedActive = false;
};
