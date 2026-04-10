import QtQuick
import "qrc:/ScootUI/qml/widgets/components" as Components

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: typeof usbStore !== "undefined" ? usbStore.status : "idle"
    property string usbStep: typeof usbStore !== "undefined" ? usbStore.step : ""
    property int usbProgress: typeof usbStore !== "undefined" ? usbStore.progress : 0
    property string usbDetail: typeof usbStore !== "undefined" ? usbStore.detail : ""

    visible: opacity > 0
    opacity: (usbStatus !== "idle" && usbStatus !== "") ? 1.0 : 0.0

    // Exit UMS via right brake tap (only when active, not processing)
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: usbStatus === "active"
        function onRightTap() {
            if (typeof usbStore !== "undefined") {
                usbStore.exitUmsMode()
            }
        }
    }

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
            text: typeof translations !== "undefined" ? translations.umsPreparing : "Preparing Storage"
            font.pixelSize: themeStore.fontTitle
            font.weight: Font.Bold
            color: "#FFFFFF"
        }

        // Active state
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "active"
            spacing: 24

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: MaterialIcon.iconUsb
                font.family: "Material Icons"
                font.pixelSize: themeStore.fontHero
                color: "#FFFFFF"
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.umsActive : "Update Mode"
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.umsConnect : "Connect to Computer"
                    font.pixelSize: themeStore.fontBody
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
                    radius: themeStore.radiusModal
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
                text: typeof translations !== "undefined" ? translations.umsProcessing : "Processing Files"
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Bold
                color: "#FFFFFF"
            }

            // Current step
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: usbStep !== ""
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
                        font.pixelSize: themeStore.fontBody
                        color: "#E6FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: stepText
                        text: usbStep
                        font.pixelSize: themeStore.fontBody
                        font.weight: Font.Medium
                        color: "#E6FFFFFF" // white 90% opacity
                    }
                }
            }

            // Per-file progress bar + detail line. Only visible while a
            // file transfer is actually streaming (progress > 0).
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: usbProgress > 0
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
                            width: parent.width * (usbProgress / 100)
                            height: parent.height
                            radius: parent.radius
                            color: "#FFFFFF"
                            Behavior on width { NumberAnimation { duration: 150 } }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: usbDetail
                        visible: usbDetail !== ""
                        font.pixelSize: themeStore.fontBody
                        color: "#99FFFFFF" // white 60%
                    }
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
                        font.pixelSize: themeStore.fontBody
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
            font.pixelSize: themeStore.fontTitle
            font.weight: Font.Bold
            color: "#FFFFFF"
        }
    }

    // Control hints (bottom) — only show exit hint during active state
    Components.ControlHints {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        visible: usbStatus === "active"
        rightAction: "Exit"
    }
}
