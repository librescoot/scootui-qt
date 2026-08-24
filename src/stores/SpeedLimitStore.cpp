#include "SpeedLimitStore.h"
#include "SpeedLimitParser.h"

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
            {QStringLiteral("roadRefs"), QStringLiteral("road-refs")},
            {QStringLiteral("roadType"), QStringLiteral("road-type")},
        },
        {}, {}
    };
}

void SpeedLimitStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    // RoadInfoService is the local authority while it is producing fresh
    // route/tile matches. Ignore the slower Redis snapshot during that window
    // so its 5-second poll cannot periodically overwrite a 1 Hz local match.
    if (m_directUpdateAge.isValid()
        && m_directUpdateAge.elapsed() < DirectAuthorityHoldMs)
        return;

    if (variable == QLatin1String("speed-limit")) {
        if (value != m_speedLimit) { m_speedLimit = value; emit speedLimitChanged(); }
    } else if (variable == QLatin1String("road-name")) {
        if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
    } else if (variable == QLatin1String("road-refs")) {
        if (value != m_roadRefs) { m_roadRefs = value; emit roadRefsChanged(); }
    } else if (variable == QLatin1String("road-type")) {
        if (value != m_roadType) { m_roadType = value; emit roadTypeChanged(); }
    }
}

void SpeedLimitStore::setSpeedLimitDirect(const QString &value)
{
    markDirectUpdate();
    const QString resolved = SpeedLimitParser::resolve(value);
    if (resolved != m_speedLimit) { m_speedLimit = resolved; emit speedLimitChanged(); }
}

void SpeedLimitStore::setRoadNameDirect(const QString &value)
{
    markDirectUpdate();
    if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
}

void SpeedLimitStore::setRoadRefsDirect(const QString &value)
{
    markDirectUpdate();
    if (value != m_roadRefs) { m_roadRefs = value; emit roadRefsChanged(); }
}

void SpeedLimitStore::setRoadTypeDirect(const QString &value)
{
    markDirectUpdate();
    if (value != m_roadType) { m_roadType = value; emit roadTypeChanged(); }
}

void SpeedLimitStore::setRoadBearingDirect(double value)
{
    markDirectUpdate();
    if (value != m_roadBearing) { m_roadBearing = value; emit roadBearingChanged(); }
}

void SpeedLimitStore::markDirectUpdate()
{
    m_directUpdateAge.start();
}
