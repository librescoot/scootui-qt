#include "EnumStrings.h"
#include "Enums.h"

namespace {

// Out-of-range ints fall through the switch and come back empty; show the
// number instead so the row still says something.
template <typename E, typename F>
QString named(int v, F toString)
{
    const QString s = toString(static_cast<E>(v));
    return s.isEmpty() ? QString::number(v) : s;
}

} // namespace

EnumStrings::EnumStrings(QObject *parent)
    : QObject(parent)
{
}

QString EnumStrings::toggle(int v) const
{
    return named<ScootEnums::Toggle>(v, ScootEnums::toggleString);
}

QString EnumStrings::blinkerState(int v) const
{
    return named<ScootEnums::BlinkerState>(v, ScootEnums::blinkerStateString);
}

QString EnumStrings::blinkerSwitch(int v) const
{
    return named<ScootEnums::BlinkerSwitch>(v, ScootEnums::blinkerSwitchString);
}

QString EnumStrings::vehicleState(int v) const
{
    return named<ScootEnums::VehicleState>(v, ScootEnums::vehicleStateString);
}

QString EnumStrings::kickstand(int v) const
{
    return named<ScootEnums::Kickstand>(v, ScootEnums::kickstandString);
}

QString EnumStrings::handleBarLockSensor(int v) const
{
    return named<ScootEnums::HandleBarLockSensor>(v, ScootEnums::handleBarLockSensorString);
}

QString EnumStrings::seatboxLock(int v) const
{
    return named<ScootEnums::SeatboxLock>(v, ScootEnums::seatboxLockString);
}

QString EnumStrings::batteryState(int v) const
{
    return named<ScootEnums::BatteryState>(v, ScootEnums::batteryStateString);
}

QString EnumStrings::gpsState(int v) const
{
    return named<ScootEnums::GpsState>(v, ScootEnums::gpsStateString);
}

QString EnumStrings::modemState(int v) const
{
    return named<ScootEnums::ModemState>(v, ScootEnums::modemStateString);
}

QString EnumStrings::connectionStatus(int v) const
{
    return named<ScootEnums::ConnectionStatus>(v, ScootEnums::connectionStatusString);
}

QString EnumStrings::chargeStatus(int v) const
{
    return named<ScootEnums::ChargeStatus>(v, ScootEnums::chargeStatusString);
}

QString EnumStrings::auxChargeStatus(int v) const
{
    return named<ScootEnums::AuxChargeStatus>(v, ScootEnums::auxChargeStatusString);
}
