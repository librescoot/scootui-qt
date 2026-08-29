import QtQuick
import ScootUI 1.0
import "../components"

// Floating telltale card, shared by cluster and map views. Sizes to the set
// of active telltales; hidden entirely when none are on.
Rectangle {
    id: panel

    readonly property bool hasVehicle: typeof vehicleStore !== "undefined"
    readonly property int vehicleState: hasVehicle ? vehicleStore.state : Scooter.VehicleState.Unknown
    readonly property bool usbDisconnected: typeof connectionStore !== "undefined"
                                            && connectionStore.usingBackupConnection
    readonly property bool engineFaultActive: typeof engineStore !== "undefined"
                                              && engineStore.faults.length > 0

    // Opening the seatbox cuts the 48V bus, so main-power off while parked is
    // expected then - suppress the power-lost branch unless the seatbox is
    // closed. Drive the ECU-fault branch from the active fault set, not the
    // fault:code snapshot, which can lag a clear and would latch the light.
    readonly property bool engineFault: {
        if (!hasVehicle) return false
        var battery0Active = typeof battery0Store !== "undefined"
                             && battery0Store.present && battery0Store.batteryState === Scooter.BatteryState.Active
        return vehicleStore.isUnableToDrive === Scooter.Toggle.On
            || engineFaultActive
            || (vehicleStore.mainPower === Scooter.Toggle.Off
                && vehicleStore.seatboxLock === Scooter.SeatboxLock.Closed
                && (vehicleState === Scooter.VehicleState.Parked
                    || vehicleState === Scooter.VehicleState.ReadyToDrive || battery0Active))
    }

    readonly property bool hazards: hasVehicle && vehicleStore.blinkerState === Scooter.BlinkerState.Both
    readonly property bool parked: vehicleState === Scooter.VehicleState.Parked

    // Turtle: a present, active pack at or below 20% caps motor power.
    readonly property bool turtle: {
        var b0 = typeof battery0Store !== "undefined" && battery0Store.present
                 && battery0Store.batteryState === Scooter.BatteryState.Active && battery0Store.charge <= 20
        var b1 = typeof battery1Store !== "undefined" && battery1Store.present
                 && battery1Store.batteryState === Scooter.BatteryState.Active && battery1Store.charge <= 20
        return b0 || b1
    }

    readonly property bool anyActive: engineFault || usbDisconnected || hazards
                                      || parked || turtle

    visible: anyActive
    width: telltaleRow.width + 16
    height: telltaleRow.height + 16
    radius: themeStore.radiusCard
    color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9)
    border.width: 1
    border.color: themeStore.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.12)

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
            blinkSource: panel.hasVehicle ? vehicleStore.blinkOpacity : -1
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
