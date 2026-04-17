import QtQuick
import ScootUI 1.0

Item {
    id: clusterBottom
    height: 60

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] ClusterBottomBar completed")

    readonly property int vehicleState: VehicleStore.state
    readonly property int blinkerState: VehicleStore.blinkerState
    readonly property int unableToDrive: VehicleStore.isUnableToDrive
    readonly property int mainPower: VehicleStore.mainPower
    readonly property int faultCode: EngineStore.faultCode
    readonly property int battery0State: Battery0Store.batteryState
    readonly property bool battery0Present: Battery0Store.present
    readonly property bool usbDisconnected: ConnectionStore.usingBackupConnection

    // BlinkerState.Both = 3. VehicleState.Parked = 4. Toggle.On = 0, Off = 1. BatteryState.Active = 3.
    //
    // engineFault lights when any of:
    //   - unable-to-drive is set by vehicle-service (existing)
    //   - ECU reports a fault (firmware codes 1-16, or synthetic 20 for comm loss)
    //   - main-power is off while battery 0 is actively supplying current
    //
    // The last condition deliberately anchors on battery0 state rather than
    // vehicle:state so we don't false-positive when the seatbox is open or
    // the battery has been removed — in both cases battery0 is not Active
    // and 48V being absent is the expected state, not a fault.
    readonly property bool engineFault: unableToDrive === 0
                                      || faultCode > 0
                                      || (mainPower === 1 && battery0State === 3 && battery0Present)

    readonly property bool hasTelltales: engineFault || usbDisconnected || blinkerState === 3 || vehicleState === 4

    // AnimatedSwitcher equivalent
    Item {
        anchors.fill: parent

        // Telltale indicators
        Row {
            id: telltaleRow
            anchors.centerIn: parent
            spacing: 8
            visible: hasTelltales
            opacity: hasTelltales ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }

            // Engine warning
            Image {
                width: 32; height: 32
                source: "qrc:/ScootUI/assets/icons/librescoot-engine-warning.svg"
                sourceSize: Qt.size(32, 32)
                visible: engineFault || usbDisconnected
            }

            // Hazards
            Image {
                width: 32; height: 32
                source: "qrc:/ScootUI/assets/icons/librescoot-hazards.svg"
                sourceSize: Qt.size(32, 32)
                visible: blinkerState === 3
                opacity: VehicleStore.blinkOpacity
            }

            // Parking brake
            Image {
                width: 32; height: 32
                source: "qrc:/ScootUI/assets/icons/librescoot-parking-brake.svg"
                sourceSize: Qt.size(32, 32)
                visible: vehicleState === 4
            }
        }

        // Power display
        PowerDisplay {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: 200
            height: 56
            visible: !hasTelltales
            opacity: !hasTelltales ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }
        }
    }
}
