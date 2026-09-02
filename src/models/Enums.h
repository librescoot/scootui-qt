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

enum class MapViewMode { View3D, View2D };
Q_ENUM_NS(MapViewMode)

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
    WaitingHibernationSeatbox, WaitingHibernationConfirm,
    HopOn, HopOnLearning
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

// Mirrors the cb-battery charge-status strings emitted by bluetooth-service:
// "charging" / "not-charging" / "unknown". Unknown covers an unrecognized BLE
// value or a field that was never reported. Order is append-only: existing
// consumers depend on Charging=0, NotCharging=1.
enum class ChargeStatus { Charging, NotCharging, Unknown };
Q_ENUM_NS(ChargeStatus)

enum class AuxChargeStatus { NotCharging, FloatCharge, AbsorptionCharge, BulkCharge };
Q_ENUM_NS(AuxChargeStatus)

enum class ScreenMode { Cluster, Map, CarPlay, Debug, About, Maintenance, AddressSelection, Simulator, NavigationSetup, Faults, UpdateModeInfo, HopOnInfo, MotionDebug, SystemInfo, UpdateChannel };
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
    if (s == QLatin1String("hop-on")) return VehicleState::HopOn;
    if (s == QLatin1String("hop-on-learning")) return VehicleState::HopOnLearning;
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
    if (s == QLatin1String("charging")) return ChargeStatus::Charging;
    if (s == QLatin1String("not-charging")) return ChargeStatus::NotCharging;
    return ChargeStatus::Unknown; // bluetooth-service "unknown", or unset/unrecognized
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

inline MapViewMode parseMapViewMode(const QString &s) {
    return (s == QLatin1String("2d")) ? MapViewMode::View2D : MapViewMode::View3D;
}

inline MapRenderMode parseMapRenderMode(const QString &s) {
    return (s == QLatin1String("raster")) ? MapRenderMode::Raster : MapRenderMode::Vector;
}

inline PowerDisplayMode parsePowerDisplayMode(const QString &s) {
    return (s == QLatin1String("amps")) ? PowerDisplayMode::Amps : PowerDisplayMode::Kw;
}

// --- Enum-to-string helpers ---
//
// The inverse of the parsers above: the wire string a value is read from.
// Values with no wire form (parser fallbacks that never round-trip, such as
// an enumerator added without a string here) yield an empty QString so the
// round-trip test catches them.

inline QString toggleString(Toggle v) {
    switch (v) {
    case Toggle::On: return QStringLiteral("on");
    case Toggle::Off: return QStringLiteral("off");
    }
    return QString();
}

inline QString blinkerStateString(BlinkerState v) {
    switch (v) {
    case BlinkerState::Off: return QStringLiteral("off");
    case BlinkerState::Left: return QStringLiteral("left");
    case BlinkerState::Right: return QStringLiteral("right");
    case BlinkerState::Both: return QStringLiteral("both");
    }
    return QString();
}

inline QString blinkerSwitchString(BlinkerSwitch v) {
    switch (v) {
    case BlinkerSwitch::Off: return QStringLiteral("off");
    case BlinkerSwitch::Left: return QStringLiteral("left");
    case BlinkerSwitch::Right: return QStringLiteral("right");
    }
    return QString();
}

inline QString vehicleStateString(VehicleState v) {
    switch (v) {
    case VehicleState::Unknown: return QStringLiteral("unknown");
    case VehicleState::StandBy: return QStringLiteral("stand-by");
    case VehicleState::ReadyToDrive: return QStringLiteral("ready-to-drive");
    case VehicleState::Off: return QStringLiteral("off");
    case VehicleState::Parked: return QStringLiteral("parked");
    case VehicleState::Booting: return QStringLiteral("booting");
    case VehicleState::ShuttingDown: return QStringLiteral("shutting-down");
    case VehicleState::Hibernating: return QStringLiteral("hibernating");
    case VehicleState::HibernatingImminent: return QStringLiteral("hibernating-imminent");
    case VehicleState::Suspending: return QStringLiteral("suspending");
    case VehicleState::SuspendingImminent: return QStringLiteral("suspending-imminent");
    case VehicleState::Updating: return QStringLiteral("updating");
    case VehicleState::WaitingSeatbox: return QStringLiteral("waiting-seatbox");
    case VehicleState::WaitingHibernation: return QStringLiteral("waiting-hibernation");
    case VehicleState::WaitingHibernationAdvanced: return QStringLiteral("waiting-hibernation-advanced");
    case VehicleState::WaitingHibernationSeatbox: return QStringLiteral("waiting-hibernation-seatbox");
    case VehicleState::WaitingHibernationConfirm: return QStringLiteral("waiting-hibernation-confirm");
    case VehicleState::HopOn: return QStringLiteral("hop-on");
    case VehicleState::HopOnLearning: return QStringLiteral("hop-on-learning");
    }
    return QString();
}

inline QString kickstandString(Kickstand v) {
    switch (v) {
    case Kickstand::Up: return QStringLiteral("up");
    case Kickstand::Down: return QStringLiteral("down");
    }
    return QString();
}

inline QString handleBarLockSensorString(HandleBarLockSensor v) {
    switch (v) {
    case HandleBarLockSensor::Locked: return QStringLiteral("locked");
    case HandleBarLockSensor::Unlocked: return QStringLiteral("unlocked");
    case HandleBarLockSensor::Unknown: return QStringLiteral("unknown");
    }
    return QString();
}

inline QString seatboxLockString(SeatboxLock v) {
    switch (v) {
    case SeatboxLock::Open: return QStringLiteral("open");
    case SeatboxLock::Closed: return QStringLiteral("closed");
    }
    return QString();
}

inline QString batteryStateString(BatteryState v) {
    switch (v) {
    case BatteryState::Unknown: return QStringLiteral("unknown");
    case BatteryState::Asleep: return QStringLiteral("asleep");
    case BatteryState::Idle: return QStringLiteral("idle");
    case BatteryState::Active: return QStringLiteral("active");
    }
    return QString();
}

inline QString gpsStateString(GpsState v) {
    switch (v) {
    case GpsState::Off: return QStringLiteral("off");
    case GpsState::Searching: return QStringLiteral("searching");
    case GpsState::FixEstablished: return QStringLiteral("fix-established");
    case GpsState::Error: return QStringLiteral("error");
    }
    return QString();
}

inline QString modemStateString(ModemState v) {
    switch (v) {
    case ModemState::Off: return QStringLiteral("off");
    case ModemState::Disconnected: return QStringLiteral("disconnected");
    case ModemState::Connected: return QStringLiteral("connected");
    }
    return QString();
}

inline QString connectionStatusString(ConnectionStatus v) {
    switch (v) {
    case ConnectionStatus::Connected: return QStringLiteral("connected");
    case ConnectionStatus::Disconnected: return QStringLiteral("disconnected");
    }
    return QString();
}

inline QString chargeStatusString(ChargeStatus v) {
    switch (v) {
    case ChargeStatus::Charging: return QStringLiteral("charging");
    case ChargeStatus::NotCharging: return QStringLiteral("not-charging");
    case ChargeStatus::Unknown: return QStringLiteral("unknown");
    }
    return QString();
}

inline QString auxChargeStatusString(AuxChargeStatus v) {
    switch (v) {
    case AuxChargeStatus::NotCharging: return QStringLiteral("not-charging");
    case AuxChargeStatus::FloatCharge: return QStringLiteral("float-charge");
    case AuxChargeStatus::AbsorptionCharge: return QStringLiteral("absorption-charge");
    case AuxChargeStatus::BulkCharge: return QStringLiteral("bulk-charge");
    }
    return QString();
}

} // namespace ScootEnums
