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
    }
}
