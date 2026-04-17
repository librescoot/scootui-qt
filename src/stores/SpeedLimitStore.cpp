#include "SpeedLimitStore.h"
#include <QRegularExpression>

// Resolve OSM maxspeed values that aren't plain integers.
// Returns the canonical form that SpeedLimitIndicator.qml expects:
// a numeric string ("50"), "none", or "" (unknown/unresolvable).
static QString resolveMaxspeed(const QString &raw)
{
    if (raw.isEmpty())
        return raw;

    // Already a plain integer — pass through.
    bool ok = false;
    raw.toInt(&ok);
    if (ok)
        return raw;

    // "none" — Autobahn-style no-limit.
    if (raw == QLatin1String("none"))
        return raw;

    // Country-specific symbolic defaults (DE: is the vast majority in
    // Germany; other country prefixes follow the same pattern).
    // https://wiki.openstreetmap.org/wiki/Key:maxspeed#Default_speed_limits
    if (raw == QLatin1String("DE:urban"))           return QStringLiteral("50");
    if (raw == QLatin1String("DE:rural"))            return QStringLiteral("100");
    if (raw == QLatin1String("DE:motorway"))         return QStringLiteral("none");
    if (raw == QLatin1String("DE:living_street"))    return QStringLiteral("10");
    if (raw == QLatin1String("DE:walk"))             return QStringLiteral("5");
    if (raw == QLatin1String("DE:zone30")
        || raw == QLatin1String("DE:zone:30"))       return QStringLiteral("30");
    if (raw == QLatin1String("DE:zone20")
        || raw == QLatin1String("DE:zone:20"))       return QStringLiteral("20");
    if (raw == QLatin1String("DE:bicycle_road"))     return QStringLiteral("30");

    // Compound values ("50;30") — take the first.
    if (raw.contains(QLatin1Char(';'))) {
        const QString first = raw.section(QLatin1Char(';'), 0, 0).trimmed();
        return resolveMaxspeed(first);
    }

    // Units suffix ("30 mph", "50 km/h") — extract the leading number.
    // Only the numeric part matters; we display in km/h by convention.
    static const QRegularExpression leadingNum(QStringLiteral("^(\\d+)"));
    const auto m = leadingNum.match(raw);
    if (m.hasMatch())
        return m.captured(1);

    // Unresolvable ("signals", "variable", freeform text) — hide.
    return QString();
}

SpeedLimitStore::SpeedLimitStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
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
    const QString resolved = resolveMaxspeed(value);
    if (resolved != m_speedLimit) { m_speedLimit = resolved; emit speedLimitChanged(); }
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
