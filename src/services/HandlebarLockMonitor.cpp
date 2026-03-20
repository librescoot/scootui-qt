#include "HandlebarLockMonitor.h"
#include "ToastService.h"
#include "stores/VehicleStore.h"
#include "models/Enums.h"

const QString HandlebarLockMonitor::ToastId = QStringLiteral("handlebar-lock-warning");

HandlebarLockMonitor::HandlebarLockMonitor(VehicleStore *vehicle, ToastService *toast,
                                             QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_toast(toast)
    , m_delayTimer(new QTimer(this))
{
    m_delayTimer->setSingleShot(true);
    m_delayTimer->setInterval(DelayMs);
    connect(m_delayTimer, &QTimer::timeout, this, [this]() {
        if (m_vehicle->isReadyToDrive() &&
            m_vehicle->handleBarLockSensor() == static_cast<int>(ScootEnums::HandleBarLockSensor::Locked)) {
            m_showing = true;
            m_toast->showPermanentWarning(tr("Handlebar lock is still engaged"), ToastId);
        }
    });

    connect(m_vehicle, &VehicleStore::stateChanged, this, &HandlebarLockMonitor::evaluate);
    connect(m_vehicle, &VehicleStore::handleBarLockSensorChanged, this, &HandlebarLockMonitor::evaluate);
}

void HandlebarLockMonitor::evaluate()
{
    bool readyToDrive = m_vehicle->isReadyToDrive();
    bool locked = m_vehicle->handleBarLockSensor() == static_cast<int>(ScootEnums::HandleBarLockSensor::Locked);

    if (readyToDrive && locked) {
        if (!m_showing && !m_delayTimer->isActive())
            m_delayTimer->start();
    } else {
        m_delayTimer->stop();
        if (m_showing) {
            m_showing = false;
            m_toast->dismiss(ToastId);
        }
    }
}
