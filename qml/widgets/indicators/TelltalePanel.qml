import QtQuick
import "../components"

// Floating telltale card, shared by cluster and map views. Sizes to the set
// of active telltales; hidden entirely when none are on.
Rectangle {
    id: panel

    readonly property bool hasVehicle: typeof vehicleStore !== "undefined"
    readonly property int vehicleState: hasVehicle ? vehicleStore.state : 0
    readonly property bool usbDisconnected: typeof connectionStore !== "undefined"
                                            && connectionStore.usingBackupConnection
    readonly property bool engineFaultActive: typeof engineStore !== "undefined"
                                              && engineStore.faults.length > 0

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
        return vehicleStore.isUnableToDrive === 0
            || engineFaultActive
            || (vehicleStore.mainPower === 1
                && vehicleStore.seatboxLock === 1
                && (vehicleState === 4 || vehicleState === 2 || battery0Active))
    }

    readonly property bool hazards: hasVehicle && vehicleStore.blinkerState === 3
    readonly property bool parked: vehicleState === 4

    // Car-style alternator warning: a main pack is active but the AUX charger
    // reports not-charging. Level-independent; mirrored by the status-bar AUX
    // warning icon and the ChargingSystemMonitor toast. Uses the red UNECE R121 /
    // ISO 7000-0247 charging-condition telltale.
    readonly property bool auxNotCharging: {
        if (typeof battery0Store === "undefined" || typeof auxBatteryStore === "undefined") return false
        var mainActive = battery0Store.present && battery0Store.charge > 0
                         && battery0Store.batteryState === 3
        var auxPresent = auxBatteryStore.voltageValid || auxBatteryStore.chargeValid
        return mainActive && auxPresent && auxBatteryStore.chargeStatus === 0
    }

    // Battery mode: slot 1 only counts when the scooter is configured for two
    // packs, so an inserted-but-unused pack cannot light a telltale.
    readonly property bool dualBattery: typeof settingsStore !== "undefined" && settingsStore.dualBattery

    // Turtle: a present, active pack at or below 20% caps motor power.
    readonly property bool turtle: {
        var b0 = typeof battery0Store !== "undefined" && battery0Store.present
                 && battery0Store.batteryState === 3 && battery0Store.charge <= 20
        var b1 = dualBattery && typeof battery1Store !== "undefined" && battery1Store.present
                 && battery1Store.batteryState === 3 && battery1Store.charge <= 20
        return b0 || b1
    }

    // Battery fault: a pack in play reports faults.
    readonly property bool batteryFault: {
        var b0 = typeof battery0Store !== "undefined" && battery0Store.present
                 && battery0Store.faults.length > 0
        var b1 = dualBattery && typeof battery1Store !== "undefined" && battery1Store.present
                 && battery1Store.faults.length > 0
        return b0 || b1
    }

    readonly property bool anyActive: engineFault || usbDisconnected || hazards
                                      || parked || turtle || auxNotCharging || batteryFault

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

        Image {
            visible: panel.auxNotCharging
            width: 32; height: 32
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
            source: "qrc:/ScootUI/assets/icons/librescoot-battery-charging-condition.svg"
        }

        IndicatorLight {
            active: panel.batteryFault
            source: "qrc:/ScootUI/assets/icons/librescoot-propulsion-battery-fault.svg"
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
