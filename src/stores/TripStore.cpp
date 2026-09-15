#include "TripStore.h"

#include "EngineStore.h"
#include "VehicleStore.h"
#include "models/Enums.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <limits>

TripStore::TripStore(MdbRepository *repo, EngineStore *engine, VehicleStore *vehicle,
                     QObject *parent)
    : SyncableStore(repo, parent)
    , m_engine(engine)
    , m_vehicle(vehicle)
{
    connect(m_vehicle, &VehicleStore::stateChanged,
            this, &TripStore::onVehicleStateChanged);

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &TripStore::onTick);

    m_resetResultTimer.setSingleShot(true);
    m_resetResultTimer.setInterval(kResetResultTimeoutMs);
    connect(&m_resetResultTimer, &QTimer::timeout, this, &TripStore::onResetResultTimeout);

    m_livenessTimer.setInterval(kLivenessPollMs);
    connect(&m_livenessTimer, &QTimer::timeout, this, &TripStore::requestLiveness);
    connect(m_repo, &MdbRepository::valueFetched, this,
            [this](const QString &key, const QString &value) {
        if (key != QLatin1String("trip:ready"))
            return;
        m_serviceReady = value == QLatin1String("1");
        updatePersistentAvailability();
    });
    connect(m_repo, &MdbRepository::connectionStateChanged, this, [this](bool connected) {
        if (!connected) {
            m_serviceReady = false;
            updatePersistentAvailability();
        }
    });
}

TripStore::~TripStore()
{
    stop();
}

SyncSettings TripStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("trip:counter"), 1000,
        {
            {QStringLiteral("apiVersion"), QStringLiteral("api-version"), true},
            {QStringLiteral("distance"), QStringLiteral("distance-m")},
            {QStringLiteral("duration"), QStringLiteral("duration-s")},
            {QStringLiteral("averageSpeed"), QStringLiteral("average-speed-kmh")},
            {QStringLiteral("resetPolicy"), QStringLiteral("reset-policy")},
            {QStringLiteral("resetAt"), QStringLiteral("reset-at")},
            {QStringLiteral("resetReason"), QStringLiteral("reset-reason")},
            {QStringLiteral("generation"), QStringLiteral("generation")},
            {QStringLiteral("status"), QStringLiteral("status")},
            {QStringLiteral("updatedAt"), QStringLiteral("updated-at")},
        }, {}, {}
    };
}

void TripStore::start()
{
    SyncableStore::start();
    requestLiveness();
    m_livenessTimer.start();
    if (m_commandResultSubscription == 0) {
        m_commandResultSubscription = m_repo->subscribe(QStringLiteral("trip:command-result"),
            [this](const QString &, const QString &message) { handleCommandResult(message); });
    }
}

void TripStore::stop()
{
    m_resetResultTimer.stop();
    m_livenessTimer.stop();
    if (m_commandResultSubscription != 0) {
        m_repo->unsubscribe(QStringLiteral("trip:command-result"), m_commandResultSubscription);
        m_commandResultSubscription = 0;
    }
    SyncableStore::stop();
}

void TripStore::beginBatchUpdate()
{
    m_apiVersionSeenInBatch = false;
}

void TripStore::endBatchUpdate()
{
    m_persistentSupported = m_apiVersionSeenInBatch;
    updatePersistentAvailability();
}

void TripStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("api-version")) {
        m_apiVersionSeenInBatch = value == QLatin1String("1");
        if (value != m_apiVersion) {
            m_apiVersion = value;
            emit counterChanged();
        }
    } else if (!m_apiVersionSeenInBatch) {
        return;
    } else if (variable == QLatin1String("distance-m")) {
        bool ok = false;
        const double meters = value.toDouble(&ok);
        if (ok && meters >= 0) {
            const double distance = meters / 1000.0;
            if (!qFuzzyCompare(1.0 + distance, 1.0 + m_distance)) {
                m_distance = distance;
                emit distanceChanged();
            }
        }
    } else if (variable == QLatin1String("duration-s")) {
        bool ok = false;
        const qint64 duration = value.toLongLong(&ok);
        if (ok && duration >= 0 && duration <= std::numeric_limits<int>::max()
            && m_duration != duration) {
            m_duration = static_cast<int>(duration);
            emit durationChanged();
        }
    } else if (variable == QLatin1String("average-speed-kmh")) {
        bool ok = false;
        const double speed = value.toDouble(&ok);
        if (ok && speed >= 0 && !qFuzzyCompare(1.0 + speed, 1.0 + m_averageSpeed)) {
            m_averageSpeed = speed;
            emit averageSpeedChanged();
        }
    } else if (variable == QLatin1String("reset-policy")) {
        if (value != m_resetPolicy) { m_resetPolicy = value; emit counterChanged(); }
    } else if (variable == QLatin1String("reset-at")) {
        bool ok = false; const qint64 parsed = value.toLongLong(&ok);
        if (ok && parsed != m_resetAt) { m_resetAt = parsed; emit counterChanged(); }
    } else if (variable == QLatin1String("reset-reason")) {
        if (value != m_resetReason) { m_resetReason = value; emit counterChanged(); }
    } else if (variable == QLatin1String("generation")) {
        bool ok = false; const qint64 parsed = value.toLongLong(&ok);
        if (ok && parsed != m_generation) { m_generation = parsed; emit counterChanged(); }
    } else if (variable == QLatin1String("status")) {
        if (value != m_status) { m_status = value; emit counterChanged(); }
    } else if (variable == QLatin1String("updated-at")) {
        bool ok = false; const qint64 parsed = value.toLongLong(&ok);
        if (ok && parsed != m_updatedAt) { m_updatedAt = parsed; emit counterChanged(); }
    }
}

void TripStore::setPersistentAvailable(bool available)
{
    if (available == m_persistentAvailable)
        return;
    m_persistentAvailable = available;
    if (available) {
        pauseTracking();
    } else {
        m_duration = 0;
        emit durationChanged();
        if (m_resetState == QLatin1String("pending")) {
            m_resetResultTimer.stop();
            setResetState(QStringLiteral("error"), QStringLiteral("Trip service unavailable"));
        }
        if (!m_persistentSupported)
            onVehicleStateChanged();
    }
    emit persistentAvailableChanged();
}

void TripStore::updatePersistentAvailability()
{
    setPersistentAvailable(m_persistentSupported && m_serviceReady);
}

void TripStore::requestLiveness()
{
    m_repo->requestValue(QStringLiteral("trip:ready"));
}

void TripStore::onVehicleStateChanged()
{
    if (m_persistentSupported)
        return;

    using S = ScootEnums::VehicleState;
    const auto state = static_cast<S>(m_vehicle->state());
    if (state == S::ReadyToDrive) {
        startTracking();
    } else if (state == S::Parked) {
        pauseTracking();
    } else {
        pauseTracking();
        m_resetPending = true;
    }
}

void TripStore::startTracking()
{
    if (m_resetPending) {
        m_distance = 0;
        m_accumulatedMs = 0;
        m_averageSpeed = 0;
        m_resetPending = false;
        emit distanceChanged();
        emit durationChanged();
        emit averageSpeedChanged();
    }
    if (m_tracking) return;
    m_tracking = true;
    m_elapsed.start();
    m_tickTimer->start();
}

void TripStore::pauseTracking()
{
    if (!m_tracking) return;
    m_tracking = false;
    m_accumulatedMs += m_elapsed.elapsed();
    m_tickTimer->stop();
}

void TripStore::reset()
{
    if (m_persistentSupported) {
        if (!m_persistentAvailable) {
            setResetState(QStringLiteral("error"), QStringLiteral("Trip service unavailable"));
            return;
        }
        if (m_resetState == QLatin1String("pending"))
            return;
        if (m_pendingResetId.isEmpty())
            m_pendingResetId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject command{{QStringLiteral("id"), m_pendingResetId},
                                  {QStringLiteral("op"), QStringLiteral("counter.reset")},
                                  {QStringLiteral("source"), QStringLiteral("scootui")},
                                  {QStringLiteral("expires-at"),
                                   QDateTime::currentMSecsSinceEpoch() + kResetResultTimeoutMs}};
        setResetState(QStringLiteral("pending"));
        m_resetResultTimer.start();
        // MdbRepository::push has no enqueue acknowledgement. A correlated
        // command result, or this bounded timeout, is therefore the outcome.
        m_repo->push(QStringLiteral("scooter:trip"),
                     QString::fromUtf8(QJsonDocument(command).toJson(QJsonDocument::Compact)));
        return;
    }

    m_distance = 0;
    m_duration = 0;
    m_accumulatedMs = 0;
    m_averageSpeed = 0;
    if (m_tracking)
        m_elapsed.restart();
    emit distanceChanged();
    emit durationChanged();
    emit averageSpeedChanged();
}

void TripStore::onTick()
{
    if (m_overrideActive || m_persistentAvailable || !m_tracking)
        return;

    const double speed = m_engine->speed();
    m_distance += speed / 3600.0;
    emit distanceChanged();

    m_duration = static_cast<int>((m_accumulatedMs + m_elapsed.elapsed()) / 1000);
    emit durationChanged();

    if (m_duration > 0) {
        m_averageSpeed = m_distance * 3600.0 / m_duration;
        emit averageSpeedChanged();
    }
}

void TripStore::onResetResultTimeout()
{
    if (m_resetState != QLatin1String("pending"))
        return;
    setResetState(QStringLiteral("error"), QStringLiteral("Trip service unavailable"));
}

void TripStore::setOverride(double distance_km, int duration_s, double avg_speed_kmh)
{
    m_overrideActive = true;
    m_overrideDistance = distance_km;
    m_overrideDuration = duration_s;
    m_overrideAverageSpeed = avg_speed_kmh;
    emit distanceChanged();
    emit durationChanged();
    emit averageSpeedChanged();
}

void TripStore::clearOverride()
{
    if (!m_overrideActive)
        return;
    m_overrideActive = false;
    if (!m_persistentAvailable) {
        m_distance = 0;
        m_duration = 0;
        m_averageSpeed = 0;
        m_accumulatedMs = 0;
        m_resetPending = true;
        if (m_tracking)
            m_elapsed.restart();
    }
    emit distanceChanged();
    emit durationChanged();
    emit averageSpeedChanged();
}

void TripStore::setResetState(const QString &state, const QString &error)
{
    if (state == m_resetState && error == m_resetError)
        return;
    m_resetState = state;
    m_resetError = error;
    emit resetStateChanged();
}

void TripStore::handleCommandResult(const QString &message)
{
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8());
    if (!document.isObject())
        return;
    const QJsonObject result = document.object();
    if (result.value(QStringLiteral("id")).toString() != m_pendingResetId
        || result.value(QStringLiteral("op")).toString() != QLatin1String("counter.reset"))
        return;

    m_resetResultTimer.stop();
    const QString status = result.value(QStringLiteral("status")).toString();
    if (status == QLatin1String("ok") || status == QLatin1String("success"))
        setResetState(QStringLiteral("success"));
    else
        setResetState(QStringLiteral("error"), result.value(QStringLiteral("error")).toString());
    m_pendingResetId.clear();
}
