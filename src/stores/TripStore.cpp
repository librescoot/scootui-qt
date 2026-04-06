#include "TripStore.h"
#include "EngineStore.h"
#include "VehicleStore.h"
#include "models/Enums.h"

TripStore::TripStore(EngineStore *engine, VehicleStore *vehicle, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_vehicle(vehicle)
{
    s_instance = this;
    connect(m_vehicle, &VehicleStore::stateChanged,
            this, &TripStore::onVehicleStateChanged);

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &TripStore::onTick);
}

void TripStore::onVehicleStateChanged()
{
    using S = ScootEnums::ScooterState;
    auto state = static_cast<S>(m_vehicle->state());

    if (state == S::ReadyToDrive) {
        startTracking();
    } else if (state == S::Parked) {
        // Pause within a session — keep metrics, will resume on next ReadyToDrive
        pauseTracking();
    } else {
        // StandBy, Hibernating, Off, ShuttingDown etc — end of session
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
        m_speedSum = 0;
        m_speedSamples = 0;
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
    m_distance = 0;
    m_accumulatedMs = 0;
    m_averageSpeed = 0;
    m_speedSum = 0;
    m_speedSamples = 0;
    if (m_tracking) {
        m_elapsed.restart();
    }
    emit distanceChanged();
    emit durationChanged();
    emit averageSpeedChanged();
}

void TripStore::onTick()
{
    if (!m_tracking) return;

    double speed = m_engine->speed();

    // Distance: speed (km/h) * 1 second = speed/3600 km
    m_distance += speed / 3600.0;
    emit distanceChanged();

    m_duration = static_cast<int>((m_accumulatedMs + m_elapsed.elapsed()) / 1000);
    emit durationChanged();

    if (speed > 0) {
        m_speedSum += speed;
        m_speedSamples++;
        m_averageSpeed = m_speedSum / m_speedSamples;
        emit averageSpeedChanged();
    }
}
