#include "NavigationStore.h"

NavigationStore::NavigationStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings NavigationStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("navigation"), 5000,
        {
            {QStringLiteral("latitude"), QStringLiteral("latitude"), true},
            {QStringLiteral("longitude"), QStringLiteral("longitude"), true},
            {QStringLiteral("address"), QStringLiteral("address"), true},
            {QStringLiteral("timestamp"), QStringLiteral("timestamp"), true},
            {QStringLiteral("destination"), QStringLiteral("destination"), true},
            {QStringLiteral("waypoints"), QStringLiteral("waypoints"), true},
            {QStringLiteral("currentStep"), QStringLiteral("current-step"), true},
            {QStringLiteral("plan"), QStringLiteral("plan"), true},
        },
        {}, {}
    };
}

void NavigationStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("latitude")) {
        if (value != m_latitude) { m_latitude = value; emit latitudeChanged(); }
    } else if (variable == QLatin1String("longitude")) {
        if (value != m_longitude) { m_longitude = value; emit longitudeChanged(); }
    } else if (variable == QLatin1String("address")) {
        if (value != m_address) { m_address = value; emit addressChanged(); }
    } else if (variable == QLatin1String("timestamp")) {
        if (value != m_timestamp) { m_timestamp = value; emit timestampChanged(); }
    } else if (variable == QLatin1String("destination")) {
        if (value != m_destination) { m_destination = value; emit destinationChanged(); }
    } else if (variable == QLatin1String("waypoints")) {
        if (value != m_waypoints) { m_waypoints = value; emit waypointsChanged(); }
    } else if (variable == QLatin1String("current-step")) {
        if (value != m_currentStep) { m_currentStep = value; emit currentStepChanged(); }
    } else if (variable == QLatin1String("plan")) {
        if (value != m_plan) { m_plan = value; emit planChanged(); }
    }
}
