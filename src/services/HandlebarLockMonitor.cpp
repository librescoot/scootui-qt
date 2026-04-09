#include "HandlebarLockMonitor.h"
#include "ToastService.h"
#include "stores/VehicleStore.h"
#include "l10n/Translations.h"
#include "models/Enums.h"

const QString HandlebarLockMonitor::ToastId = QStringLiteral("handlebar-lock-warning");

HandlebarLockMonitor::HandlebarLockMonitor(VehicleStore *vehicle, ToastService *toast,
                                             Translations *translations, QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_toast(toast)
    , m_translations(translations)
    , m_delayTimer(new QTimer(this))
{
    m_delayTimer->setSingleShot(true);
    m_delayTimer->setInterval(DelayMs);
    connect(m_delayTimer, &QTimer::timeout, this, [this]() {
        auto state = static_cast<ScootEnums::VehicleState>(m_vehicle->state());
        bool activeState = (state == ScootEnums::VehicleState::ReadyToDrive ||
                            state == ScootEnums::VehicleState::Parked);
        if (activeState &&
            m_vehicle->handleBarLockSensor() == static_cast<int>(ScootEnums::HandleBarLockSensor::Locked)) {
            m_showing = true;
            m_toast->showPermanentWarning(m_translations->warningHandlebarLocked(), ToastId);
        }
    });

    connect(m_vehicle, &VehicleStore::stateChanged, this, &HandlebarLockMonitor::evaluate);
    connect(m_vehicle, &VehicleStore::handleBarLockSensorChanged, this, &HandlebarLockMonitor::evaluate);
}

void HandlebarLockMonitor::evaluate()
{
    auto state = static_cast<ScootEnums::VehicleState>(m_vehicle->state());
    bool activeState = (state == ScootEnums::VehicleState::ReadyToDrive ||
                        state == ScootEnums::VehicleState::Parked);
    auto lockSensor = static_cast<ScootEnums::HandleBarLockSensor>(m_vehicle->handleBarLockSensor());

    if (lockSensor == ScootEnums::HandleBarLockSensor::Unknown)
        return;

    if (activeState && lockSensor == ScootEnums::HandleBarLockSensor::Locked) {
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
