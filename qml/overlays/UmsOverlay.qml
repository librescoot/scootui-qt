import QtQuick
import "../widgets/components"
import ScootUI 1.0

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: UsbStore.status
    property string usbStep: UsbStore.step
    property int usbProgress: UsbStore.progress
    property string usbDetail: UsbStore.detail

    visible: opacity > 0
    opacity: (umsOverlay.usbStatus !== "idle" && umsOverlay.usbStatus !== "") ? 1.0 : 0.0

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
            visible: umsOverlay.usbStatus === "preparing"
            text: Translations.umsPreparing
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
            color: "#FFFFFF"
        }

        // Active state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: umsOverlay.usbStatus === "active"
            spacing: 24

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: MaterialIcon.iconUsb
                font.family: "Material Icons"
                font.pixelSize: ThemeStore.fontHero
                color: "#FFFFFF"
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.umsActive
                    font.pixelSize: ThemeStore.fontHeading
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.umsConnect
                    font.pixelSize: ThemeStore.fontBody
                    color: "#B3FFFFFF" // white 70% opacity
                }
            }
        }

        // Processing state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: umsOverlay.usbStatus === "processing"
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
                    radius: ThemeStore.radiusModal
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
                        running: umsOverlay.usbStatus === "processing"
                        from: 0; to: 360
                        duration: 1000
                        loops: Animation.Infinite
                    }
                }
            }

            Item { width: 1; height: 24 }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Translations.umsProcessing
                font.pixelSize: ThemeStore.fontTitle
                font.weight: Font.Bold
                color: "#FFFFFF"
            }

            // Current step
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: umsOverlay.usbStep !== ""
                width: parent.width
                height: stepRow.height + 12

                Row {
                    id: stepRow
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    spacing: 4

                    Text {
                        text: MaterialIcon.iconArrowForward
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontBody
                        color: "#E6FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: stepText
                        text: umsOverlay.usbStep
                        font.pixelSize: ThemeStore.fontBody
                        font.weight: Font.Medium
                        color: "#E6FFFFFF" // white 90% opacity
                    }
                }
            }

            // Per-file progress bar + detail line. Only visible while a
            // file transfer is actually streaming (progress > 0).
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: umsOverlay.usbProgress > 0
                width: Math.min(umsOverlay.width - 96, 400)
                height: progressBarCol.height + 8

                Column {
                    id: progressBarCol
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    width: parent.width
                    spacing: 6

                    // Track
                    Rectangle {
                        width: parent.width
                        height: 4
                        radius: 2
                        color: "#33FFFFFF" // white 20%

                        // Fill
                        Rectangle {
                            width: parent.width * (umsOverlay.usbProgress / 100)
                            height: parent.height
                            radius: parent.radius
                            color: "#FFFFFF"
                            Behavior on width { NumberAnimation { duration: 150 } }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: umsOverlay.usbDetail
                        visible: umsOverlay.usbDetail !== ""
                        font.pixelSize: ThemeStore.fontBody
                        color: "#99FFFFFF" // white 60%
                    }
                }
            }

            // Log entries
            Item { width: 1; height: 16 }

            Column {
                id: logColumn
                anchors.horizontalCenter: parent.horizontalCenter
                width: umsOverlay.width - 48
                spacing: 2

                Repeater {
                    model: true ? UmsLogStore.logEntries : []

                    Text {
                        required property string modelData
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData
                        font.pixelSize: ThemeStore.fontBody
                        color: "#80FFFFFF" // white 50% opacity
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }
            }
        }

        // Default/other state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: umsOverlay.usbStatus !== "preparing" && umsOverlay.usbStatus !== "active"
                     && umsOverlay.usbStatus !== "processing" && umsOverlay.usbStatus !== "idle"
                     && umsOverlay.usbStatus !== ""
            text: umsOverlay.usbStatus
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
            color: "#FFFFFF"
        }
    }

    // Control hints (bottom) — only show exit hint during active state
    ControlHints {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        visible: umsOverlay.usbStatus === "active"
        leftLabel: Translations.controlLeftBrakeHold
        leftAction: Translations.umsHoldExit
    }
}
