#include "MotionStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

MotionStore::MotionStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

MotionStore::~MotionStore()
{
    if (m_headingSubscriptionId != 0) {
        m_repo->unsubscribe(QStringLiteral("motion:heading"), m_headingSubscriptionId);
        m_headingSubscriptionId = 0;
    }
    if (m_sensorsSubscriptionId != 0) {
        m_repo->unsubscribe(QStringLiteral("motion:sensors"), m_sensorsSubscriptionId);
        m_sensorsSubscriptionId = 0;
    }
}

void MotionStore::start()
{
    SyncableStore::start();

    if (m_headingSubscriptionId == 0) {
        m_headingSubscriptionId = m_repo->subscribe(
            QStringLiteral("motion:heading"), [this](const QString &, const QString &message) {
                applyHeadingSnapshot(message);
            });
    }

    if (m_sensorsSubscriptionId == 0) {
        m_sensorsSubscriptionId = m_repo->subscribe(
            QStringLiteral("motion:sensors"), [this](const QString &, const QString &message) {
                applySensorsSnapshot(message);
            });
    }
}

void MotionStore::stop()
{
    if (m_headingSubscriptionId != 0) {
        m_repo->unsubscribe(QStringLiteral("motion:heading"), m_headingSubscriptionId);
        m_headingSubscriptionId = 0;
    }
    if (m_sensorsSubscriptionId != 0) {
        m_repo->unsubscribe(QStringLiteral("motion:sensors"), m_sensorsSubscriptionId);
        m_sensorsSubscriptionId = 0;
    }
    SyncableStore::stop();
}

SyncSettings MotionStore::syncSettings() const
{
    // Slow HGETALL safety net so heading-related hash fields are visible
    // even when no pub/sub message has arrived since startup. The actual
    // live data flows over kHeadingChannel / kSensorsChannel.
    return SyncSettings{
        QStringLiteral("motion"), 5000,
        {
            {QStringLiteral("heading-deg"), QStringLiteral("heading-deg")},
            {QStringLiteral("heading-accuracy"), QStringLiteral("heading-accuracy")},
            {QStringLiteral("heading-tilt"), QStringLiteral("heading-tilt")},
            {QStringLiteral("heading-tilt-comp"), QStringLiteral("heading-tilt-comp")},
        },
        {}, {}
    };
}

void MotionStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    // Hash-derived fields. Only consume the ones that aren't continuously
    // updated by the pub/sub channels — the snapshot path is authoritative
    // for the rest.
    if (variable == QLatin1String("heading-deg") && m_headingTimestamp == 0) {
        bool ok;
        const double v = value.toDouble(&ok);
        if (ok && v != m_headingDeg) {
            m_headingDeg = v;
            emit headingChanged();
        }
    }
}

static double readDouble(const QJsonObject &obj, const QString &key, double fallback = 0.0)
{
    const auto v = obj.value(key);
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) return v.toString().toDouble();
    return fallback;
}

static qint64 readInt64(const QJsonObject &obj, const QString &key, qint64 fallback = 0)
{
    const auto v = obj.value(key);
    if (v.isDouble()) return static_cast<qint64>(v.toDouble());
    if (v.isString()) return v.toString().toLongLong();
    return fallback;
}

void MotionStore::applyHeadingSnapshot(const QString &payload)
{
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const auto obj = doc.object();

    m_headingTimestamp = readInt64(obj, QStringLiteral("timestamp"));
    m_headingDeg = readDouble(obj, QStringLiteral("heading_deg"));
    m_headingRawDeg = readDouble(obj, QStringLiteral("heading_raw_deg"));
    m_headingFastDeg = readDouble(obj, QStringLiteral("heading_fast_deg"), m_headingDeg);
    m_headingSlowDeg = readDouble(obj, QStringLiteral("heading_slow_deg"), m_headingDeg);
    m_accuracyDeg = readDouble(obj, QStringLiteral("accuracy_deg"));
    m_tiltCompensated = obj.value(QStringLiteral("tilt_compensated")).toBool();
    m_tiltDeg = readDouble(obj, QStringLiteral("tilt_deg"));
    m_magStrengthUT = readDouble(obj, QStringLiteral("mag_strength_ut"));
    m_excessG = readDouble(obj, QStringLiteral("excess_g"));
    m_yawRateDPS = readDouble(obj, QStringLiteral("yaw_rate_dps"));

    emit headingChanged();
}

void MotionStore::applySensorsSnapshot(const QString &payload)
{
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const auto obj = doc.object();

    m_sensorsTimestamp = readInt64(obj, QStringLiteral("timestamp"));

    if (const auto a = obj.value(QStringLiteral("accel")).toObject(); !a.isEmpty()) {
        m_accelX = readDouble(a, QStringLiteral("x"));
        m_accelY = readDouble(a, QStringLiteral("y"));
        m_accelZ = readDouble(a, QStringLiteral("z"));
        m_accelMagnitude = readDouble(a, QStringLiteral("magnitude"));
    }
    if (const auto g = obj.value(QStringLiteral("gyro")).toObject(); !g.isEmpty()) {
        m_gyroX = readDouble(g, QStringLiteral("x"));
        m_gyroY = readDouble(g, QStringLiteral("y"));
        m_gyroZ = readDouble(g, QStringLiteral("z"));
        m_gyroMagnitude = readDouble(g, QStringLiteral("magnitude"));
    }
    if (const auto m = obj.value(QStringLiteral("mag")).toObject(); !m.isEmpty()) {
        m_magX = readDouble(m, QStringLiteral("x"));
        m_magY = readDouble(m, QStringLiteral("y"));
        m_magZ = readDouble(m, QStringLiteral("z"));
        m_magMagnitude = readDouble(m, QStringLiteral("magnitude"));
    }

    emit sensorsChanged();
}
