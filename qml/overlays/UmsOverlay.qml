import QtQuick

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: typeof usbStore !== "undefined" ? usbStore.status : "idle"
    property string usbStep: typeof usbStore !== "undefined" ? usbStore.step : ""

    visible: opacity > 0
    opacity: (usbStatus !== "idle" && usbStatus !== "") ? 1.0 : 0.0

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        // Preparing state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "preparing"
            text: "Preparing Storage"
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }

        // Active state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "active"
            spacing: 24

            // Flutter: Icons.usb, size: 64
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\ue697" // usb
                font.family: "Material Icons"
                font.pixelSize: 64
                color: "#FFFFFF"
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "USB Mass Storage"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#FFFFFF"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Connect to Computer"
                    font.pixelSize: 16
                    color: "#B3FFFFFF" // white 70% opacity
                }
            }
        }

        // Processing state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "processing"
            spacing: 0

            // Spinner
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 40
                height: 40

                Rectangle {
                    id: spinnerArc
                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    radius: 18
                    color: "transparent"
                    border.color: "#FFFFFF"
                    border.width: 3

                    Rectangle {
                        width: parent.width / 2
                        height: parent.height / 2
                        color: "#000000"
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                    }

                    RotationAnimator on rotation {
                        running: usbStatus === "processing"
                        from: 0; to: 360
                        duration: 1000
                        loops: Animation.Infinite
                    }
                }
            }

            Item { width: 1; height: 24 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Processing Files"
                font.pixelSize: 20
                font.bold: true
                color: "#FFFFFF"
            }

            // Current step
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: usbStep !== ""
                width: parent.width
                height: stepText.height + 12

                Text {
                    id: stepText
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    text: "\u2192 " + usbStep
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: "#E6FFFFFF" // white 90% opacity
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // Log entries
            Item { width: 1; height: 16 }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2

                Repeater {
                    model: typeof umsLogStore !== "undefined" ? umsLogStore.logEntries : []

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData
                        font.pixelSize: 12
                        color: "#80FFFFFF" // white 50% opacity
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        width: umsOverlay.width - 48
                    }
                }
            }
        }

        // Default/other state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus !== "preparing" && usbStatus !== "active"
                     && usbStatus !== "processing" && usbStatus !== "idle"
                     && usbStatus !== ""
            text: usbStatus
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
        }
    }
}
