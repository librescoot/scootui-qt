#include "GpsStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {
constexpr auto kTpvChannel = "gps:tpv";
}

GpsStore::GpsStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

GpsStore::~GpsStore()
{
    if (m_tpvSubscribed) {
        m_repo->unsubscribe(QStringLiteral("gps:tpv"));
        m_tpvSubscribed = false;
    }
}

void GpsStore::start()
{
    SyncableStore::start();

    // Subscribe to the gps:tpv pub/sub channel for full TPV snapshots pushed
    // by modem-service. This bypasses the HGETALL polling roundtrip — every
    // message is a complete view of the GPS state. The base-class poll is
    // kept as a low-rate safety net (see syncSettings) and to prime initial
    // state on startup before the first push arrives.
    m_repo->subscribe(QStringLiteral("gps:tpv"),
                      [this](const QString &, const QString &message) {
                          applySnapshot(message);
                      });
    m_tpvSubscribed = true;
}

void GpsStore::stop()
{
    if (m_tpvSubscribed) {
        m_repo->unsubscribe(QStringLiteral("gps:tpv"));
        m_tpvSubscribed = false;
    }
    SyncableStore::stop();
}

void GpsStore::applySnapshot(const QString &payload)
{
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const auto obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString &key = it.key();
        const QJsonValue v = it.value();
        QString s;
        if (v.isString())
            s = v.toString();
        else if (v.isBool())
            s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (v.isDouble())
            s = QString::number(v.toDouble(), 'f', -1);
        else
            continue;
        applyFieldUpdate(key, s);
    }
}

SyncSettings GpsStore::syncSettings() const
{
    return SyncSettings{
        // Pub/sub via gps:tpv (subscribed in start()) carries pushed TPV
        // snapshots at 1 Hz. The HGETALL poll here is a low-rate safety net
        // — it covers the brief window between subscribe and the first push,
        // and absorbs any rare missed messages. 5 s is well within the staleness
        // tolerances downstream (MapService age compensation, hasRecentFix at
        // 20 s).
        QStringLiteral("gps"), 5000,
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
            {QStringLiteral("active"), QStringLiteral("active")},
            {QStringLiteral("connected"), QStringLiteral("connected")},
        },
        {}, {}
    };
}

void GpsStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("latitude")) {
        auto v = value.toDouble();
        if (v != m_latitude) {
            bool wasValid = hasValidGps();
            m_latitude = v;
            emit latitudeChanged();
            if (hasValidGps() != wasValid) emit hasValidGpsChanged();
        }
    } else if (variable == QLatin1String("longitude")) {
        auto v = value.toDouble();
        if (v != m_longitude) {
            bool wasValid = hasValidGps();
            m_longitude = v;
            emit longitudeChanged();
            if (hasValidGps() != wasValid) emit hasValidGpsChanged();
        }
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
    } else if (variable == QLatin1String("active")) {
        bool v = (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
        if (v != m_active) { m_active = v; emit activeChanged(); }
    } else if (variable == QLatin1String("connected")) {
        bool v = (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
        if (v != m_connected) { m_connected = v; emit connectedChanged(); }
    }
}
