#include "GpsStore.h"

GpsStore::GpsStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
    s_instance = this;
}

SyncSettings GpsStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("gps"), 1000,
        {
            {QStringLiteral("latitude"), QStringLiteral("latitude")},
            {QStringLiteral("longitude"), QStringLiteral("longitude")},
            {QStringLiteral("course"), QStringLiteral("course")},
            {QStringLiteral("speed"), QStringLiteral("speed")},
            {QStringLiteral("altitude"), QStringLiteral("altitude")},
            {QStringLiteral("updated"), QStringLiteral("updated")},
            {QStringLiteral("timestamp"), QStringLiteral("timestamp")},
            {QStringLiteral("state"), QStringLiteral("state")},
            {QStringLiteral("eph"), QStringLiteral("eph")},
            {QStringLiteral("ept"), QStringLiteral("ept")},
            {QStringLiteral("eps"), QStringLiteral("eps")},
            {QStringLiteral("snr"), QStringLiteral("snr")},
            {QStringLiteral("pdop"), QStringLiteral("pdop")},
            {QStringLiteral("hdop"), QStringLiteral("hdop")},
            {QStringLiteral("vdop"), QStringLiteral("vdop")},
            {QStringLiteral("satellites-used"), QStringLiteral("satellites-used")},
            {QStringLiteral("satellites-visible"), QStringLiteral("satellites-visible")},
            {QStringLiteral("fix"), QStringLiteral("fix")},
            {QStringLiteral("mode"), QStringLiteral("mode")},
            {QStringLiteral("last_ttff_seconds"), QStringLiteral("last_ttff_seconds")},
            {QStringLiteral("last_ttff_mode"), QStringLiteral("last_ttff_mode")},
        },
        {}, {}
    };
}

void GpsStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("latitude")) {
        auto v = value.toDouble();
        if (v != m_latitude) { m_latitude = v; emit latitudeChanged(); }
    } else if (variable == QLatin1String("longitude")) {
        auto v = value.toDouble();
        if (v != m_longitude) { m_longitude = v; emit longitudeChanged(); }
    } else if (variable == QLatin1String("course")) {
        auto v = value.toDouble();
        if (v != m_course) { m_course = v; emit courseChanged(); }
    } else if (variable == QLatin1String("speed")) {
        auto v = value.toDouble();
        if (v != m_speed) { m_speed = v; emit speedChanged(); }
    } else if (variable == QLatin1String("altitude")) {
        auto v = value.toDouble();
        if (v != m_altitude) { m_altitude = v; emit altitudeChanged(); }
    } else if (variable == QLatin1String("updated")) {
        if (value != m_updated) { m_updated = value; emit updatedChanged(); }
    } else if (variable == QLatin1String("timestamp")) {
        if (value != m_timestamp) { m_timestamp = value; m_timestampAge.restart(); emit timestampChanged(); }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseGpsState(value);
        if (v != m_gpsState) { m_gpsState = v; emit gpsStateChanged(); }
    } else if (variable == QLatin1String("eph")) {
        auto v = value.toDouble();
        if (v != m_eph) { m_eph = v; emit ephChanged(); }
    } else if (variable == QLatin1String("ept")) {
        auto v = value.toDouble();
        if (v != m_ept) { m_ept = v; emit eptChanged(); }
    } else if (variable == QLatin1String("eps")) {
        auto v = value.toDouble();
        if (v != m_eps) { m_eps = v; emit epsChanged(); }
    } else if (variable == QLatin1String("snr")) {
        auto v = value.toDouble();
        if (v != m_snr) { m_snr = v; emit snrChanged(); }
    } else if (variable == QLatin1String("pdop")) {
        auto v = value.toDouble();
        if (v != m_pdop) { m_pdop = v; emit pdopChanged(); }
    } else if (variable == QLatin1String("hdop")) {
        auto v = value.toDouble();
        if (v != m_hdop) { m_hdop = v; emit hdopChanged(); }
    } else if (variable == QLatin1String("vdop")) {
        auto v = value.toDouble();
        if (v != m_vdop) { m_vdop = v; emit vdopChanged(); }
    } else if (variable == QLatin1String("satellites-used")) {
        auto v = value.toInt();
        if (v != m_satellitesUsed) { m_satellitesUsed = v; emit satellitesUsedChanged(); }
    } else if (variable == QLatin1String("satellites-visible")) {
        auto v = value.toInt();
        if (v != m_satellitesVisible) { m_satellitesVisible = v; emit satellitesVisibleChanged(); }
    } else if (variable == QLatin1String("fix")) {
        if (value != m_fix) { m_fix = value; emit fixChanged(); }
    } else if (variable == QLatin1String("mode")) {
        if (value != m_mode) { m_mode = value; emit modeChanged(); }
    } else if (variable == QLatin1String("last_ttff_seconds")) {
        auto v = value.toDouble();
        if (v != m_lastTtffSeconds) { m_lastTtffSeconds = v; emit lastTtffSecondsChanged(); }
    } else if (variable == QLatin1String("last_ttff_mode")) {
        if (value != m_lastTtffMode) { m_lastTtffMode = value; emit lastTtffModeChanged(); }
    }
}
