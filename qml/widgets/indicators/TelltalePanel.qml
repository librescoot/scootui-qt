import QtQuick
import "../components"
import ScootUI 1.0

// Floating telltale card, shared by cluster and map views. Sizes to the set
// of active telltales; hidden entirely when none are on.
Rectangle {
    id: panel

    readonly property bool hasVehicle: typeof VehicleStore !== "undefined"
    readonly property int vehicleState: hasVehicle ? VehicleStore.state : 0
    readonly property bool usbDisconnected: typeof ConnectionStore !== "undefined"
                                            && ConnectionStore.usingBackupConnection
    readonly property bool engineFaultActive: typeof EngineStore !== "undefined"
                                              && EngineStore.faults.length > 0

    // VehicleState: ReadyToDrive = 2, Parked = 4. Toggle: On = 0, Off = 1.
    // BatteryState: Active = 3. SeatboxLock: Open = 0, Closed = 1.
    // Opening the seatbox cuts the 48V bus, so main-power off while parked is
    // expected then - suppress the power-lost branch unless the seatbox is
    // closed. Drive the ECU-fault branch from the active fault set, not the
    // fault:code snapshot, which can lag a clear and would latch the light.
    readonly property bool engineFault: {
        if (!hasVehicle) return false
        var battery0Active = typeof battery0Store !== "undefined"
                             && battery0Store.present && battery0Store.batteryState === 3
        return VehicleStore.isUnableToDrive === 0
            || engineFaultActive
            || (VehicleStore.mainPower === 1
                && VehicleStore.seatboxLock === 1
                && (vehicleState === 4 || vehicleState === 2 || battery0Active))
    }

    readonly property bool hazards: hasVehicle && VehicleStore.blinkerState === 3
    readonly property bool parked: vehicleState === 4

    // Turtle: a present, active pack at or below 20% caps motor power.
    readonly property bool turtle: {
        var b0 = typeof battery0Store !== "undefined" && battery0Store.present
                 && battery0Store.batteryState === 3 && battery0Store.charge <= 20
        var b1 = typeof battery1Store !== "undefined" && battery1Store.present
                 && battery1Store.batteryState === 3 && battery1Store.charge <= 20
        return b0 || b1
    }

    readonly property bool anyActive: engineFault || usbDisconnected || hazards
                                      || parked || turtle

    visible: anyActive
    width: telltaleRow.width + 16
    height: telltaleRow.height + 16
    radius: ThemeStore.radiusCard
    color: ThemeStore.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9)
    border.width: 1
    border.color: ThemeStore.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.12)

    Row {
        id: telltaleRow
        anchors.centerIn: parent
        spacing: 8

        IndicatorLight {
            active: panel.engineFault || panel.usbDisconnected
            source: "qrc:/ScootUI/assets/icons/librescoot-engine-warning.svg"
        }

        IndicatorLight {
            active: panel.hazards
            blinking: true
            blinkSource: panel.hasVehicle ? VehicleStore.blinkOpacity : -1
            source: "qrc:/ScootUI/assets/icons/librescoot-hazards.svg"
        }

        IndicatorLight {
            active: panel.parked
            source: "qrc:/ScootUI/assets/icons/librescoot-parking-brake.svg"
        }

        TintedImage {
            visible: panel.turtle
            width: 32; height: 32
            tintColor: "#FFC107"
            source: "qrc:/ScootUI/assets/icons/librescoot-turtle-mode.svg"
        }
    }
}
