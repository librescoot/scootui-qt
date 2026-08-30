#include "SystemHealthMonitor.h"

#include "stores/VehicleStore.h"
#include "stores/ConnectionStore.h"
#include "services/ToastService.h"
#include "l10n/Translations.h"
#include "models/Enums.h"

namespace {

const QString kRedisToastId = QStringLiteral("redis-disconnect");
const QString kUsbToastId = QStringLiteral("usb-disconnect");

// States the dashboard renders normally; anything else is a fault or an
// update flow the maintenance screen should front.
bool stateAllowed(int state)
{
    switch (static_cast<ScootEnums::VehicleState>(state)) {
    case ScootEnums::VehicleState::Unknown:
    case ScootEnums::VehicleState::ReadyToDrive:
    case ScootEnums::VehicleState::Parked:
    case ScootEnums::VehicleState::ShuttingDown:
    case ScootEnums::VehicleState::WaitingHibernation:
    case ScootEnums::VehicleState::WaitingHibernationAdvanced:
    case ScootEnums::VehicleState::WaitingHibernationSeatbox:
    case ScootEnums::VehicleState::WaitingHibernationConfirm:
        return true;
    default:
        return false;
    }
}

}

SystemHealthMonitor::SystemHealthMonitor(VehicleStore *vehicle, ConnectionStore *connection,
                                         ToastService *toasts, Translations *translations,
                                         QObject *parent)
    : QObject(parent)
    , m_vehicle(vehicle)
    , m_connection(connection)
    , m_toasts(toasts)
    , m_translations(translations)
{
    m_startupGrace.setSingleShot(true);
    m_startupGrace.setInterval(kStartupGraceMs);
    connect(&m_startupGrace, &QTimer::timeout, this, [this]() {
        m_startupGraceElapsed = true;
        emit healthChanged();
    });
    m_startupGrace.start();

    connect(m_vehicle, &VehicleStore::stateChanged,
            this, &SystemHealthMonitor::onVehicleStateChanged);
    connect(m_connection, &ConnectionStore::prolongedDisconnectChanged,
            this, &SystemHealthMonitor::onProlongedDisconnectChanged);
    connect(m_connection, &ConnectionStore::hasEverConnectedChanged,
            this, &SystemHealthMonitor::healthChanged);
    connect(m_connection, &ConnectionStore::usingBackupConnectionChanged,
            this, &SystemHealthMonitor::onUsingBackupConnectionChanged);
}

bool SystemHealthMonitor::neverConnected() const
{
    return m_connection->prolongedDisconnect() && !m_connection->hasEverConnected();
}

bool SystemHealthMonitor::unknownPastGrace() const
{
    return m_vehicle->state() == static_cast<int>(ScootEnums::VehicleState::Unknown)
        && m_startupGraceElapsed;
}

bool SystemHealthMonitor::showMaintenance() const
{
    if (neverConnected())
        return true;
    if (!stateAllowed(m_vehicle->state()))
        return true;
    return unknownPastGrace();
}

bool SystemHealthMonitor::showConnectionInfo() const
{
    return neverConnected() || unknownPastGrace();
}

void SystemHealthMonitor::onVehicleStateChanged()
{
    // A known state ends the startup grace question for good.
    if (m_vehicle->state() != static_cast<int>(ScootEnums::VehicleState::Unknown))
        m_startupGrace.stop();
    emit healthChanged();
}

void SystemHealthMonitor::onProlongedDisconnectChanged()
{
    // Mid-session loss gets a permanent toast; loss before the first
    // connection is the maintenance screen's job instead.
    if (m_connection->prolongedDisconnect() && m_connection->hasEverConnected())
        m_toasts->showPermanentError(m_translations->redisDisconnected(), kRedisToastId);
    else
        m_toasts->dismiss(kRedisToastId);
    emit healthChanged();
}

void SystemHealthMonitor::onUsingBackupConnectionChanged()
{
    if (m_connection->usingBackupConnection())
        m_toasts->showPermanentError(m_translations->usbDisconnected(), kUsbToastId);
    else
        m_toasts->dismiss(kUsbToastId);
}
