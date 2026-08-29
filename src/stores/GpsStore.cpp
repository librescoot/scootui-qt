#include "GpsStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <cstdint>
#include "repositories/RedisSchema.h"

namespace {
constexpr auto kTpvChannel = "gps:tpv";
}

GpsStore::GpsStore(MdbRepository *repo, QObject *parent,
                   qint64 recentFixThresholdMs)
    : SyncableStore(repo, parent)
    , m_recentFixThresholdMs(recentFixThresholdMs)
{
    m_recentFixExpiry.setSingleShot(true);
    connect(&m_recentFixExpiry, &QTimer::timeout,
            this, &GpsStore::recentFixChanged);
}

GpsStore::~GpsStore()
{
    if (m_tpvSubscribed) {
        m_repo->unsubscribe(RedisSchema::channel::GpsTpv);
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
    m_repo->subscribe(RedisSchema::channel::GpsTpv,
                      [this](const QString &, const QString &message) {
                          applySnapshot(message);
                      });
    m_tpvSubscribed = true;
}

void GpsStore::stop()
{
    if (m_tpvSubscribed) {
        m_repo->unsubscribe(RedisSchema::channel::GpsTpv);
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

    beginBatchUpdate();
    m_forceSample = true;
    const auto obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString &key = it.key();
        const QJsonValue v = it.value();
        QString s;
        if (v.isString()) {
            s = v.toString();
        } else if (v.isBool()) {
            s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (v.isDouble()) {
            // Integer-valued JSON numbers (satellites-used / -visible) must
            // round-trip as "8", not "8.000000" — applyFieldUpdate parses
            // those fields with toInt(), which stops at the decimal point
            // and silently returns 0. That zeroed every TPV snapshot, with
            // the 5 s HGETALL poll briefly restoring the real value.
            const double d = v.toDouble();
            const qint64 i = static_cast<qint64>(d);
            if (static_cast<double>(i) == d && std::abs(d) < 1e15)
                s = QString::number(i);
            else
                s = QString::number(d, 'g', 17);
        } else {
            continue;
        }
        applyFieldUpdate(key, s);
    }
    endBatchUpdate();
}

void GpsStore::beginBatchUpdate()
{
    if (m_batchDepth == 0) {
        m_batchWasValid = hasValidGps();
        m_batchWasRecent = hasRecentFix();
        m_batchAggregatePending = true;
    }
    ++m_batchDepth;
}

void GpsStore::endBatchUpdate()
{
    finishBatch();
}

void GpsStore::finishBatch(bool forceSample)
{
    m_forceSample = m_forceSample || forceSample;
    if (m_batchDepth > 0)
        --m_batchDepth;
    if (m_batchDepth != 0)
        return;

    if (m_sampleDirty || m_forceSample)
        emit sampleChanged();
    if (m_batchAggregatePending) {
        if (hasValidGps() != m_batchWasValid)
            emit hasValidGpsChanged();
        if (hasRecentFix() != m_batchWasRecent)
            emit recentFixChanged();
        m_batchAggregatePending = false;
    }
    m_sampleDirty = false;
    m_forceSample = false;
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
            if (m_batchDepth == 0 && hasValidGps() != wasValid)
                emit hasValidGpsChanged();
            m_sampleDirty = true;
        }
    } else if (variable == QLatin1String("longitude")) {
        auto v = value.toDouble();
        if (v != m_longitude) {
            bool wasValid = hasValidGps();
            m_longitude = v;
            emit longitudeChanged();
            if (m_batchDepth == 0 && hasValidGps() != wasValid)
                emit hasValidGpsChanged();
            m_sampleDirty = true;
        }
    } else if (variable == QLatin1String("course")) {
        auto v = value.toDouble();
        if (v != m_course) { m_course = v; emit courseChanged(); m_sampleDirty = true; }
    } else if (variable == QLatin1String("speed")) {
        auto v = value.toDouble();
        if (v != m_speed) { m_speed = v; emit speedChanged(); m_sampleDirty = true; }
    } else if (variable == QLatin1String("altitude")) {
        auto v = value.toDouble();
        if (v != m_altitude) { m_altitude = v; emit altitudeChanged(); }
    } else if (variable == QLatin1String("updated")) {
        if (value != m_updated) { m_updated = value; emit updatedChanged(); }
    } else if (variable == QLatin1String("timestamp")) {
        if (value != m_timestamp) {
            bool wasRecent = hasRecentFix();
            bool wasValid = hasValidGps();
            m_timestamp = value;
            m_timestampAge.restart();
            m_recentFixExpiry.start(static_cast<int>(m_recentFixThresholdMs + 1));
            emit timestampChanged();
            if (m_batchDepth == 0 && hasRecentFix() != wasRecent)
                emit recentFixChanged();
            if (m_batchDepth == 0 && hasValidGps() != wasValid)
                emit hasValidGpsChanged();
            m_sampleDirty = true;
        }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseGpsState(value);
        if (v != m_gpsState) {
            bool wasRecent = hasRecentFix();
            m_gpsState = v;
            emit gpsStateChanged();
            if (m_batchDepth == 0 && hasRecentFix() != wasRecent)
                emit recentFixChanged();
            m_sampleDirty = true;
        }
    } else if (variable == QLatin1String("eph")) {
        auto v = value.toDouble();
        if (v != m_eph) { m_eph = v; emit ephChanged(); m_sampleDirty = true; }
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
        if (v != m_hdop) { m_hdop = v; emit hdopChanged(); m_sampleDirty = true; }
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

    if (m_batchDepth == 0 && m_sampleDirty)
        finishBatch();
}
