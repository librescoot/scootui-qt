#include "SpeedLimitStore.h"
#include "RoadSignStyle.h"
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
        m_speedLimitSource = Source::External;
        if (value != m_speedLimit) { m_speedLimit = value; emit speedLimitChanged(); }
    } else if (variable == QLatin1String("road-name")) {
        m_roadNameSource = Source::External;
        if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
    } else if (variable == QLatin1String("road-refs")) {
        m_roadRefsSource = Source::External;
        if (value != m_roadRefs) {
            m_roadNetworksSource = Source::External;
            m_roadNetworks.clear();
            m_roadRefs = value;
            emit roadRefsChanged();
            updateRoadSignStyle();
        }
    } else if (variable == QLatin1String("road-type")) {
        m_roadTypeSource = Source::External;
        if (value != m_roadType) {
            m_roadNetworksSource = Source::External;
            m_roadNetworks.clear();
            m_roadType = value;
            emit roadTypeChanged();
            updateRoadSignStyle();
        }
    }
}

void SpeedLimitStore::setSpeedLimitDirect(const QString &value, Source source)
{
    m_speedLimitSource = source;
    markDirectUpdate(source);
    const QString resolved = SpeedLimitParser::resolve(value);
    if (resolved != m_speedLimit) { m_speedLimit = resolved; emit speedLimitChanged(); }
}

void SpeedLimitStore::setRoadNameDirect(const QString &value, Source source)
{
    m_roadNameSource = source;
    markDirectUpdate(source);
    if (value != m_roadName) { m_roadName = value; emit roadNameChanged(); }
}

void SpeedLimitStore::setRoadRefsDirect(const QString &value, Source source)
{
    m_roadRefsSource = source;
    markDirectUpdate(source);
    if (value != m_roadRefs) {
        m_roadRefs = value;
        emit roadRefsChanged();
        updateRoadSignStyle();
    }
}

void SpeedLimitStore::setRoadTypeDirect(const QString &value, Source source)
{
    m_roadTypeSource = source;
    markDirectUpdate(source);
    if (value != m_roadType) {
        m_roadType = value;
        emit roadTypeChanged();
        updateRoadSignStyle();
    }
}

void SpeedLimitStore::setRoadNetworksDirect(const QString &value, Source source)
{
    m_roadNetworksSource = source;
    markDirectUpdate(source);
    if (value != m_roadNetworks) {
        m_roadNetworks = value;
        updateRoadSignStyle();
    }
}

void SpeedLimitStore::setRoadBearingDirect(double value, Source source)
{
    m_roadBearingSource = source;
    markDirectUpdate(source);
    if (value != m_roadBearing) { m_roadBearing = value; emit roadBearingChanged(); }
}

void SpeedLimitStore::clearSource(Source source)
{
    if (m_speedLimitSource == source)
        setSpeedLimitDirect(QString(), Source::External);
    if (m_roadNameSource == source)
        setRoadNameDirect(QString(), Source::External);
    if (m_roadRefsSource == source)
        setRoadRefsDirect(QString(), Source::External);
    if (m_roadTypeSource == source)
        setRoadTypeDirect(QString(), Source::External);
    if (m_roadNetworksSource == source)
        setRoadNetworksDirect(QString(), Source::External);
    if (m_roadBearingSource == source)
        setRoadBearingDirect(-1, Source::External);
}

void SpeedLimitStore::markDirectUpdate(Source source)
{
    // Clearing expired output is not a fresh local publication and must not
    // renew the hold that suppresses external updates.
    if (source != Source::External)
        m_directUpdateAge.start();
}

void SpeedLimitStore::updateRoadSignStyle()
{
    const QString style = RoadSignStyle::classify(
        m_roadType, m_roadRefs, m_roadNetworks);
    if (style != m_roadSignStyle) {
        m_roadSignStyle = style;
        emit roadSignStyleChanged();
    }
}
