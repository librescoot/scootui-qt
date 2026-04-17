import QtQuick
import "../power"

Item {
    id: clusterBottom
    height: 60

    readonly property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    readonly property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : 0
    readonly property int unableToDrive: typeof vehicleStore !== "undefined" ? vehicleStore.isUnableToDrive : 0
    readonly property int mainPower: typeof vehicleStore !== "undefined" ? vehicleStore.mainPower : 1
    readonly property int faultCode: typeof engineStore !== "undefined" ? engineStore.faultCode : 0
    readonly property int battery0State: typeof battery0Store !== "undefined" ? battery0Store.batteryState : 0
    readonly property bool battery0Present: typeof battery0Store !== "undefined" && battery0Store.present
    readonly property bool usbDisconnected: typeof connectionStore !== "undefined" && connectionStore.usingBackupConnection

    // VehicleState: Parked = 4, ReadyToDrive = 2. BlinkerState.Both = 3. Toggle.On = 0, Off = 1. BatteryState.Active = 3.
    readonly property bool engineFault: unableToDrive === 0
                                      || faultCode > 0
                                      || (mainPower === 1
                                          && (vehicleState === 4
                                              || vehicleState === 2
                                              || (battery0State === 3 && battery0Present)))

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
                opacity: typeof vehicleStore !== "undefined" ? vehicleStore.blinkOpacity : 1
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
