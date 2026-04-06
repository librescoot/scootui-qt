import QtQuick
import "../power"
import ScootUI

Item {
    id: clusterBottom
    height: 60

    readonly property int vehicleState: VehicleStore.state
    readonly property int blinkerState: VehicleStore.blinkerState
    readonly property int unableToDrive: VehicleStore.isUnableToDrive
    readonly property bool usbDisconnected: ConnectionStore.usingBackupConnection

    // ScooterState.Parked = 4, BlinkerState.Both = 3, Toggle.On = 0
    readonly property bool hasTelltales: unableToDrive === 0 || usbDisconnected || blinkerState === 3 || vehicleState === 4

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
                visible: unableToDrive === 0 || usbDisconnected // Toggle.On or USB fallback
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
