#pragma once

#include <QObject>
#include <QString>
#include <QHash>

namespace ScootEnums {
Q_NAMESPACE
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class ConnectionStatus { Connected, Disconnected };
Q_ENUM_NS(ConnectionStatus)

enum class Toggle { On, Off };
Q_ENUM_NS(Toggle)

enum class MapType { Online, Offline };
Q_ENUM_NS(MapType)

enum class MapRenderMode { Vector, Raster };
Q_ENUM_NS(MapRenderMode)

enum class PowerDisplayMode { Kw, Amps };
Q_ENUM_NS(PowerDisplayMode)

enum class BlinkerState { Off, Left, Right, Both };
Q_ENUM_NS(BlinkerState)

enum class BlinkerSwitch { Off, Left, Right };
Q_ENUM_NS(BlinkerSwitch)

enum class VehicleState {
    Unknown, StandBy, ReadyToDrive, Off, Parked,
    Booting, ShuttingDown, Hibernating, HibernatingImminent,
    Suspending, SuspendingImminent, Updating,
    WaitingSeatbox, WaitingHibernation, WaitingHibernationAdvanced,
    WaitingHibernationSeatbox, WaitingHibernationConfirm
};
Q_ENUM_NS(VehicleState)

enum class Kickstand { Up, Down };
Q_ENUM_NS(Kickstand)

enum class HandleBarLockSensor { Locked, Unlocked, Unknown };
Q_ENUM_NS(HandleBarLockSensor)

enum class SeatboxLock { Open, Closed };
Q_ENUM_NS(SeatboxLock)

enum class BatteryState { Unknown, Asleep, Idle, Active };
Q_ENUM_NS(BatteryState)

enum class GpsState { Off, Searching, FixEstablished, Error };
Q_ENUM_NS(GpsState)

enum class ModemState { Off, Disconnected, Connected };
Q_ENUM_NS(ModemState)

enum class ChargeStatus { Charging, NotCharging };
Q_ENUM_NS(ChargeStatus)

enum class AuxChargeStatus { NotCharging, FloatCharge, AbsorptionCharge, BulkCharge };
Q_ENUM_NS(AuxChargeStatus)

enum class ScreenMode { Cluster, Map, CarPlay, Debug, About, Maintenance, Ota, AddressSelection, Simulator, NavigationSetup, Destination, Faults, UpdateModeInfo, HopOnInfo };
Q_ENUM_NS(ScreenMode)

enum class SetupMode { DisplayMaps, Routing, Both };
Q_ENUM_NS(SetupMode)

enum class MapDownloadStatus { Idle, CheckingUpdates, Locating, Downloading, Installing, Done, Error };
Q_ENUM_NS(MapDownloadStatus)

// --- String-to-enum parsing helpers ---

inline Toggle parseToggle(const QString &s) {
    return (s == QLatin1String("on")) ? Toggle::On : Toggle::Off;
}

inline BlinkerState parseBlinkerState(const QString &s) {
    if (s == QLatin1String("left")) return BlinkerState::Left;
    if (s == QLatin1String("right")) return BlinkerState::Right;
    if (s == QLatin1String("both")) return BlinkerState::Both;
    return BlinkerState::Off;
}

inline BlinkerSwitch parseBlinkerSwitch(const QString &s) {
    if (s == QLatin1String("left")) return BlinkerSwitch::Left;
    if (s == QLatin1String("right")) return BlinkerSwitch::Right;
    return BlinkerSwitch::Off;
}

inline VehicleState parseVehicleState(const QString &s) {
    if (s == QLatin1String("stand-by")) return VehicleState::StandBy;
    if (s == QLatin1String("ready-to-drive")) return VehicleState::ReadyToDrive;
    if (s == QLatin1String("off")) return VehicleState::Off;
    if (s == QLatin1String("parked")) return VehicleState::Parked;
    if (s == QLatin1String("booting")) return VehicleState::Booting;
    if (s == QLatin1String("shutting-down")) return VehicleState::ShuttingDown;
    if (s == QLatin1String("hibernating")) return VehicleState::Hibernating;
    if (s == QLatin1String("hibernating-imminent")) return VehicleState::HibernatingImminent;
    if (s == QLatin1String("suspending")) return VehicleState::Suspending;
    if (s == QLatin1String("suspending-imminent")) return VehicleState::SuspendingImminent;
    if (s == QLatin1String("updating")) return VehicleState::Updating;
    if (s == QLatin1String("waiting-seatbox")) return VehicleState::WaitingSeatbox;
    if (s == QLatin1String("waiting-hibernation")) return VehicleState::WaitingHibernation;
    if (s == QLatin1String("waiting-hibernation-advanced")) return VehicleState::WaitingHibernationAdvanced;
    if (s == QLatin1String("waiting-hibernation-seatbox")) return VehicleState::WaitingHibernationSeatbox;
    if (s == QLatin1String("waiting-hibernation-confirm")) return VehicleState::WaitingHibernationConfirm;
    return VehicleState::Unknown;
}

inline Kickstand parseKickstand(const QString &s) {
    return (s == QLatin1String("up")) ? Kickstand::Up : Kickstand::Down;
}

inline HandleBarLockSensor parseHandleBarLockSensor(const QString &s) {
    if (s == QLatin1String("locked")) return HandleBarLockSensor::Locked;
    if (s == QLatin1String("unlocked")) return HandleBarLockSensor::Unlocked;
    return HandleBarLockSensor::Unknown;
}

inline SeatboxLock parseSeatboxLock(const QString &s) {
    return (s == QLatin1String("open")) ? SeatboxLock::Open : SeatboxLock::Closed;
}

inline BatteryState parseBatteryState(const QString &s) {
    if (s == QLatin1String("asleep")) return BatteryState::Asleep;
    if (s == QLatin1String("idle")) return BatteryState::Idle;
    if (s == QLatin1String("active")) return BatteryState::Active;
    return BatteryState::Unknown;
}

inline GpsState parseGpsState(const QString &s) {
    if (s == QLatin1String("searching")) return GpsState::Searching;
    if (s == QLatin1String("fix-established")) return GpsState::FixEstablished;
    if (s == QLatin1String("error")) return GpsState::Error;
    return GpsState::Off;
}

inline ModemState parseModemState(const QString &s) {
    if (s == QLatin1String("connected")) return ModemState::Connected;
    if (s == QLatin1String("disconnected")) return ModemState::Disconnected;
    return ModemState::Off;
}

inline ConnectionStatus parseConnectionStatus(const QString &s) {
    return (s == QLatin1String("connected")) ? ConnectionStatus::Connected : ConnectionStatus::Disconnected;
}

inline ChargeStatus parseChargeStatus(const QString &s) {
    return (s == QLatin1String("charging")) ? ChargeStatus::Charging : ChargeStatus::NotCharging;
}

inline AuxChargeStatus parseAuxChargeStatus(const QString &s) {
    if (s == QLatin1String("float-charge")) return AuxChargeStatus::FloatCharge;
    if (s == QLatin1String("absorption-charge")) return AuxChargeStatus::AbsorptionCharge;
    if (s == QLatin1String("bulk-charge")) return AuxChargeStatus::BulkCharge;
    return AuxChargeStatus::NotCharging;
}

inline MapType parseMapType(const QString &s) {
    return (s == QLatin1String("offline")) ? MapType::Offline : MapType::Online;
}

inline MapRenderMode parseMapRenderMode(const QString &s) {
    return (s == QLatin1String("raster")) ? MapRenderMode::Raster : MapRenderMode::Vector;
}

inline PowerDisplayMode parsePowerDisplayMode(const QString &s) {
    return (s == QLatin1String("amps")) ? PowerDisplayMode::Amps : PowerDisplayMode::Kw;
}

} // namespace ScootEnums
