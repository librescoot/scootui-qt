import QtQuick
import ScootUI 1.0

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: typeof UsbStore !== "undefined" ? UsbStore.status : "idle"
    property string usbStep: typeof UsbStore !== "undefined" ? UsbStore.step : ""
    property int usbProgress: typeof UsbStore !== "undefined" ? UsbStore.progress : 0
    property string usbDetail: typeof UsbStore !== "undefined" ? UsbStore.detail : ""

    readonly property bool isDark: typeof ThemeStore !== "undefined" ? ThemeStore.isDark : true
    readonly property color bgColor: isDark ? "#000000" : "#FFFFFF"
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    // 90% / 70% / 60% / 50% ramp, mirrored per theme
    readonly property color textStep: isDark ? "#E6FFFFFF" : "#E6000000"
    readonly property color textConnect: isDark ? "#B3FFFFFF" : "#B3000000"
    readonly property color textDetail: isDark ? "#99FFFFFF" : "#99000000"
    readonly property color textLog: isDark ? "#80FFFFFF" : "#80000000"
    readonly property color trackColor: isDark ? "#33FFFFFF" : "#33000000"

    // Latched when the rider holds the left brake during preparing.
    // ums-service abandons the entry at the end of its copy work, but that
    // is seconds away and it keeps publishing until then, so hide the
    // overlay now rather than leave a screen the rider has already
    // dismissed. Cleared once ums-service comes back to idle.
    property bool cancelPending: false

    onUsbStatusChanged: {
        if (usbStatus === "idle" || usbStatus === "")
            cancelPending = false
    }

    Connections {
        target: InputHandler
        // Keyed on brake:left:hold, the same 3s gesture ums-service exits
        // on, so the two sides cannot disagree about what the rider did.
        function onLeftBrakeHold() {
            if (umsOverlay.usbStatus === "preparing")
                umsOverlay.cancelPending = true
        }
    }

    visible: opacity > 0
    opacity: (usbStatus !== "idle" && usbStatus !== "" && !cancelPending) ? 1.0 : 0.0

    Rectangle {
        anchors.fill: parent
        color: umsOverlay.bgColor
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        // Preparing state
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "preparing"
            text: typeof Translations !== "undefined" ? Translations.umsPreparing : "Preparing Storage"
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
            color: umsOverlay.textPrimary
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
                font.pixelSize: ThemeStore.fontHero
                color: umsOverlay.textPrimary
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof Translations !== "undefined" ? Translations.umsActive : "Update Mode"
                    font.pixelSize: ThemeStore.fontHeading
                    font.weight: Font.Bold
                    color: umsOverlay.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof Translations !== "undefined" ? Translations.umsConnect : "Connect to Computer"
                    font.pixelSize: ThemeStore.fontBody
                    color: umsOverlay.textConnect
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
                    radius: ThemeStore.radiusModal
                    color: "transparent"
                    border.color: umsOverlay.textPrimary
                    border.width: 3

                    Rectangle {
                        width: parent.width / 2
                        height: parent.height / 2
                        color: umsOverlay.bgColor
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
                text: typeof Translations !== "undefined" ? Translations.umsProcessing : "Processing Files"
                font.pixelSize: ThemeStore.fontTitle
                font.weight: Font.Bold
                color: umsOverlay.textPrimary
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
                        font.pixelSize: ThemeStore.fontBody
                        color: umsOverlay.textStep
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: stepText
                        text: usbStep
                        font.pixelSize: ThemeStore.fontBody
                        font.weight: Font.Medium
                        color: umsOverlay.textStep
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
                        color: umsOverlay.trackColor

                        // Fill
                        Rectangle {
                            width: parent.width * (usbProgress / 100)
                            height: parent.height
                            radius: parent.radius
                            color: umsOverlay.textPrimary
                            Behavior on width { NumberAnimation { duration: 150 } }
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: usbDetail
                        visible: usbDetail !== ""
                        font.pixelSize: ThemeStore.fontBody
                        color: umsOverlay.textDetail
                    }
                }
            }

            // Log entries
            Item { width: 1; height: 16 }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2

                Repeater {
                    model: typeof UmsLogStore !== "undefined" ? UmsLogStore.logEntries : []

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData
                        font.pixelSize: ThemeStore.fontBody
                        color: umsOverlay.textLog
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
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
            color: umsOverlay.textPrimary
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
        visible: usbStatus === "active" || usbStatus === "preparing"
        leftLabel: typeof Translations !== "undefined" ? Translations.controlLeftBrakeHold : "Left Brake (Hold)"
        leftAction: typeof Translations !== "undefined"
                    ? (usbStatus === "preparing" ? Translations.controlCancel : Translations.umsHoldExit)
                    : (usbStatus === "preparing" ? "Cancel" : "Exit")
    }
}
