#pragma once

#include <QString>

// The Redis surface scootui-qt touches, in one place. Hash names double as
// the pub/sub channel SyncableStore listens on. Per-store field names stay in
// each store's syncSettings(), where every field occurs exactly once; a field
// belongs here only once two files need to agree on it.
namespace RedisSchema {

// Hashes (HGET/HSET/HGETALL and the same-named refresh channel).
namespace hash {
inline const QString Vehicle     = QStringLiteral("vehicle");
inline const QString EngineEcu   = QStringLiteral("engine-ecu");
inline const QString Settings    = QStringLiteral("settings");
inline const QString Dashboard   = QStringLiteral("dashboard");
inline const QString Navigation  = QStringLiteral("navigation");
inline const QString Internet    = QStringLiteral("internet");
inline const QString Modem       = QStringLiteral("modem");
inline const QString Ble         = QStringLiteral("ble");
inline const QString Motion      = QStringLiteral("motion");
inline const QString Ota         = QStringLiteral("ota");
inline const QString Scooter     = QStringLiteral("scooter");
inline const QString Usb         = QStringLiteral("usb");
inline const QString Gps         = QStringLiteral("gps");
inline const QString AuxBattery  = QStringLiteral("aux-battery");
inline const QString CbBattery   = QStringLiteral("cb-battery");
inline const QString SpeedLimit  = QStringLiteral("speed-limit");
inline const QString System      = QStringLiteral("system");
inline const QString VersionMdb  = QStringLiteral("version:mdb");
inline const QString VersionDbc  = QStringLiteral("version:dbc");

inline QString battery(const QString &slot) { return QStringLiteral("battery:") + slot; }
}

// Command lists (LPUSH here, an MDB service BRPOPs them). usb:log is the
// exception: ums-service appends and scootui only reads.
namespace list {
inline const QString ScooterState     = QStringLiteral("scooter:state");
inline const QString ScooterBlinker   = QStringLiteral("scooter:blinker");
inline const QString ScooterHopOn     = QStringLiteral("scooter:hop-on");
inline const QString ScooterBluetooth = QStringLiteral("scooter:bluetooth");
inline const QString ScooterDbcHold   = QStringLiteral("scooter:dbc-hold");
inline const QString ScooterUpdateMdb = QStringLiteral("scooter:update:mdb");
inline const QString ScooterUpdateDbc = QStringLiteral("scooter:update:dbc");
inline const QString SettingsOverlay  = QStringLiteral("settings:overlay");
inline const QString UsbLog           = QStringLiteral("usb:log");
}

// Pub/sub channels that are not hash refresh channels.
namespace channel {
inline const QString ScootuiCommand = QStringLiteral("scootui:command");
inline const QString Buttons        = QStringLiteral("buttons");
inline const QString InputEvents    = QStringLiteral("input-events");
inline const QString GpsTpv         = QStringLiteral("gps:tpv");
inline const QString MotionSensors  = QStringLiteral("motion:sensors");
inline const QString MotionHeading  = QStringLiteral("motion:heading");
}

}
