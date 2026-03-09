#include "VehicleStore.h"

VehicleStore::VehicleStore(MdbRepository *repo, QObject *parent)
    : SyncableStore(repo, parent)
{
}

SyncSettings VehicleStore::syncSettings() const
{
    return SyncSettings{
        QStringLiteral("vehicle"),
        1000,
        {
            {QStringLiteral("blinkerState"), QStringLiteral("blinker:state")},
            {QStringLiteral("blinkerSwitch"), QStringLiteral("blinker:switch")},
            {QStringLiteral("brakeLeft"), QStringLiteral("brake:left")},
            {QStringLiteral("brakeRight"), QStringLiteral("brake:right")},
            {QStringLiteral("kickstand"), QStringLiteral("kickstand")},
            {QStringLiteral("state"), QStringLiteral("state")},
            {QStringLiteral("handleBarLockSensor"), QStringLiteral("handlebar:lock-sensor")},
            {QStringLiteral("handleBarPosition"), QStringLiteral("handlebar:position")},
            {QStringLiteral("seatboxButton"), QStringLiteral("seatbox:button")},
            {QStringLiteral("seatboxLock"), QStringLiteral("seatbox:lock")},
            {QStringLiteral("hornButton"), QStringLiteral("horn-button")},
            {QStringLiteral("isUnableToDrive"), QStringLiteral("unable-to-drive")},
        },
        {},
        {}
    };
}

void VehicleStore::applyFieldUpdate(const QString &variable, const QString &value)
{
    if (variable == QLatin1String("blinker:state")) {
        auto v = ScootEnums::parseBlinkerState(value);
        if (v != m_blinkerState) { m_blinkerState = v; emit blinkerStateChanged(); }
    } else if (variable == QLatin1String("blinker:switch")) {
        auto v = ScootEnums::parseBlinkerSwitch(value);
        if (v != m_blinkerSwitch) { m_blinkerSwitch = v; emit blinkerSwitchChanged(); }
    } else if (variable == QLatin1String("brake:left")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_brakeLeft) { m_brakeLeft = v; emit brakeLeftChanged(); }
    } else if (variable == QLatin1String("brake:right")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_brakeRight) { m_brakeRight = v; emit brakeRightChanged(); }
    } else if (variable == QLatin1String("kickstand")) {
        auto v = ScootEnums::parseKickstand(value);
        if (v != m_kickstand) { m_kickstand = v; emit kickstandChanged(); }
    } else if (variable == QLatin1String("state")) {
        auto v = ScootEnums::parseScooterState(value);
        if (v != m_state) { m_state = v; emit stateChanged(); }
        if (value != m_stateRaw) { m_stateRaw = value; emit stateRawChanged(); }
    } else if (variable == QLatin1String("handlebar:lock-sensor")) {
        auto v = ScootEnums::parseHandleBarLockSensor(value);
        if (v != m_handleBarLockSensor) { m_handleBarLockSensor = v; emit handleBarLockSensorChanged(); }
    } else if (variable == QLatin1String("handlebar:position")) {
        auto v = ScootEnums::parseHandleBarPosition(value);
        if (v != m_handleBarPosition) { m_handleBarPosition = v; emit handleBarPositionChanged(); }
    } else if (variable == QLatin1String("seatbox:button")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_seatboxButton) { m_seatboxButton = v; emit seatboxButtonChanged(); }
    } else if (variable == QLatin1String("seatbox:lock")) {
        auto v = ScootEnums::parseSeatboxLock(value);
        if (v != m_seatboxLock) { m_seatboxLock = v; emit seatboxLockChanged(); }
    } else if (variable == QLatin1String("horn-button")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_hornButton) { m_hornButton = v; emit hornButtonChanged(); }
    } else if (variable == QLatin1String("unable-to-drive")) {
        auto v = ScootEnums::parseToggle(value);
        if (v != m_isUnableToDrive) { m_isUnableToDrive = v; emit isUnableToDriveChanged(); }
    }
}
