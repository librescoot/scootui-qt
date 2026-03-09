import QtQuick
import QtQuick.Layouts

Rectangle {
    id: maintenanceScreen
    color: "black"

    property bool showConnectionInfo: false
    property string stateRaw: typeof vehicleStore !== "undefined" ? vehicleStore.stateRaw : ""

    // After 5 seconds, switch to connection info mode
    Timer {
        interval: 5000
        running: !maintenanceScreen.showConnectionInfo
        onTriggered: maintenanceScreen.showConnectionInfo = true
    }

    // --- Loading mode (default) ---
    Item {
        id: loadingMode
        anchors.fill: parent
        visible: !showConnectionInfo

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            // Rotating spinner
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: 32
                height: 32

                Rectangle {
                    id: spinner
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "white"
                    border.width: 3
                    radius: 16

                    // Mask out part of the circle to look like a spinner arc
                    Rectangle {
                        width: 18
                        height: 18
                        color: "black"
                        anchors.right: parent.right
                        anchors.top: parent.top
                    }

                    RotationAnimation on rotation {
                        from: 0
                        to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: loadingMode.visible
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Connecting..."
                color: "white"
                font.pixelSize: 16
            }
        }
    }

    // --- Connection info mode ---
    Item {
        id: connectionInfoMode
        anchors.fill: parent
        visible: showConnectionInfo

        ColumnLayout {
            anchors.centerIn: parent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: "Trying to connect to vehicle system..."
                color: "white"
                font.pixelSize: 20
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.preferredHeight: 20 }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.24)
            }

            Item { Layout.preferredHeight: 20 }

            Text {
                Layout.fillWidth: true
                text: "This usually indicates a missing or unreliable connection between " +
                      "the dashboard computer (DBC) and the middle driver board (MDB). " +
                      "Check the USB cable if this persists."
                color: Qt.rgba(1, 1, 1, 0.70)
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.preferredHeight: 20 }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.24)
            }

            Item { Layout.preferredHeight: 20 }

            Text {
                Layout.fillWidth: true
                text: "To put your scooter into drive mode anyway, raise the kickstand, " +
                      "hold both brakes and press the seatbox button."
                color: Qt.rgba(1, 1, 1, 0.60)
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // --- State raw indicator at bottom ---
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        text: maintenanceScreen.stateRaw
        color: Qt.rgba(1, 1, 1, 0.54)
        font.pixelSize: 12
        visible: maintenanceScreen.stateRaw.length > 0
    }
}
