#include "SpeedLimitStore.h"

SpeedLimitStore::SpeedLimitStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings SpeedLimitStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("speed-limit"), 5000,
        {
            {QStringLiteral("speedLimit"), QStringLiteral("speed-limit")},
            {QStringLiteral("roadName"), QStringLiteral("road-name")},
            {QStringLiteral("roadType"), QStringLiteral("road-type")},
        },
        {}, {}
    };
}

void SpeedLimitStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("speed-limit")) {
        if (value != m_speedLimit) { m_speedLimit = value; emit speedLimitChanged(); }
    } else if (variable == QLatin1String("road-name")) {
        if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
    } else if (variable == QLatin1String("road-type")) {
        if (value != m_roadType) { m_roadType = value; emit roadTypeChanged(); }
    }
}

void SpeedLimitStore::setSpeedLimitDirect(const QString &value)
{
    if (value != m_speedLimit) { m_speedLimit = value; emit speedLimitChanged(); }
}

void SpeedLimitStore::setRoadNameDirect(const QString &value)
{
    if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
}

void SpeedLimitStore::setRoadTypeDirect(const QString &value)
{
    if (value != m_roadType) { m_roadType = value; emit roadTypeChanged(); }
}

void SpeedLimitStore::setRoadBearingDirect(double value)
{
    if (value != m_roadBearing) { m_roadBearing = value; emit roadBearingChanged(); }
}
