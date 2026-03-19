#include "ShutdownStore.h"
#include "VehicleStore.h"

#include <QDebug>

ShutdownStore::ShutdownStore(QObject *parent)
    : QObject(parent)
{
}

void ShutdownStore::connectToVehicle(VehicleStore *vehicle)
{
    m_vehicle = vehicle;
    connect(vehicle, &VehicleStore::stateChanged,
            this, &ShutdownStore::onVehicleStateChanged);
    m_wasInDriveState = vehicle->isParked() || vehicle->isReadyToDrive();
}

void ShutdownStore::onVehicleStateChanged()
{
    if (!m_vehicle) return;

    if (m_vehicle->isShuttingDown() && m_wasInDriveState && !m_isShuttingDown) {
        beginShutdown();
    }

    m_wasInDriveState = m_vehicle->isParked() || m_vehicle->isReadyToDrive();
}

void ShutdownStore::beginShutdown()
{
    if (!m_isShuttingDown) {
        m_isShuttingDown = true;
        emit shuttingDownChanged();
        qDebug() << "Shutdown initiated";
    }
}

void ShutdownStore::forceBlackout()
{
    if (!m_showBlackout) {
        m_isShuttingDown = true;
        m_showBlackout = true;
        emit shuttingDownChanged();
        emit showBlackoutChanged();
        qDebug() << "SIGTERM: forced blackout";
    }
}
