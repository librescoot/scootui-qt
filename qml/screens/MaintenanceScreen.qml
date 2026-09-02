import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: maintenanceScreen
    color: "black"

    property bool showConnectionInfo: false
    readonly property bool hopOnActive: typeof vehicleStore !== "undefined"
        && (vehicleStore.state === Scooter.VehicleState.HopOn
            || vehicleStore.state === Scooter.VehicleState.HopOnLearning)
    // The screen may briefly outlive a state change while Loader replaces it.
    // Never let its independent timeout override HopOnStore's backlight owner.
    readonly property bool canDisableBacklight: visible && !hopOnActive

    // Steps per second for the spinner below, and the rotation each step
    // applies. Every visual change costs one frame and a frame costs about the
    // same whatever is in it, so this number, not the drawing, is what the
    // spinner costs: on the DBC each step per second is roughly half a percent
    // of a core. 12 x 30 degrees keeps one revolution per second.
    readonly property int spinnerStepsPerSecond: 12
    readonly property int spinnerStepDegrees: 30
    property string stateRaw: typeof vehicleStore !== "undefined" ? vehicleStore.stateRaw : ""

    // Turn off backlight after 15s to save power during unattended maintenance/updates
    Timer {
        id: backlightTimer
        interval: 15000
        running: maintenanceScreen.canDisableBacklight
        onTriggered: {
            if (maintenanceScreen.canDisableBacklight
                    && typeof dashboardStore !== "undefined")
                dashboardStore.setBacklightEnabled(false)
        }
    }

    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onStateChanged() {
            if (typeof dashboardStore !== "undefined")
                dashboardStore.setBacklightEnabled(true)
        }
    }

    Component.onDestruction: {
        if (typeof dashboardStore !== "undefined")
            dashboardStore.setBacklightEnabled(true)
    }

    // --- Loading mode (default): silent spinner + optional OTA progress ---
    Item {
        id: loadingMode
        anchors.fill: parent
        visible: !showConnectionInfo

        readonly property bool otaActive: typeof dashboardStore !== "undefined" && otaStore.isActive
        readonly property string otaStatus: typeof dashboardStore !== "undefined" ? otaStore.dbcStatus : "idle"
        readonly property int otaDownloadProgress: typeof dashboardStore !== "undefined" ? otaStore.dbcDownloadProgress : 0
        readonly property int otaInstallProgress: typeof dashboardStore !== "undefined" ? otaStore.dbcInstallProgress : 0
        readonly property string otaVersion: typeof dashboardStore !== "undefined" ? otaStore.dbcUpdateVersion : ""

        Column {
            anchors.centerIn: parent
            spacing: 16

            // Spinner
            Item {
                width: 32
                height: 32
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    id: spinner
                    anchors.fill: parent
                    color: "transparent"
                    border.color: "white"
                    border.width: 3
                    radius: themeStore.radiusModal

                    Rectangle {
                        width: 18
                        height: 18
                        color: "black"
                        anchors.right: parent.right
                        anchors.top: parent.top
                    }

                    // Stepped rather than a continuous RotationAnimation: that
                    // asked for a frame every vsync, 59 a second, and cost 18%
                    // of a core with the renderer reporting no work at all.
                    Timer {
                        interval: 1000 / maintenanceScreen.spinnerStepsPerSecond
                        repeat: true
                        running: loadingMode.visible
                        onTriggered: spinner.rotation =
                            (spinner.rotation + maintenanceScreen.spinnerStepDegrees) % 360
                    }
                }
            }

            // OTA progress (below spinner, only when update active)
            Column {
                spacing: 6
                visible: loadingMode.otaActive && loadingMode.otaStatus !== "idle"
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: maintenanceScreen.width - 64
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: themeStore.fontBody
                    color: Qt.rgba(1, 1, 1, 0.8)
                    text: {
                        var tr = typeof translations !== "undefined" ? translations : null
                        switch (loadingMode.otaStatus) {
                            case "downloading": return tr ? tr.otaDownloadingUpdates : "Downloading update..."
                            case "preparing": return tr ? tr.otaPreparingUpdate : "Preparing update..."
                            case "installing": return tr ? tr.otaInstallingUpdates : "Installing update..."
                            case "pending-reboot": return tr ? tr.otaPendingReboot : "Update installed, will apply next time the scooter is started"
                            case "error": return tr ? tr.otaUpdateError : "Update failed"
                            default: return tr ? tr.otaInitializing : "Updating..."
                        }
                    }
                }

                // Progress bar
                Item {
                    width: 160
                    height: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: loadingMode.otaStatus === "downloading"
                             || loadingMode.otaStatus === "preparing"
                             || loadingMode.otaStatus === "installing"

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(1, 1, 1, 0.2)
                        radius: 1
                    }
                    Rectangle {
                        width: parent.width * (loadingMode.otaStatus === "downloading"
                               ? loadingMode.otaDownloadProgress
                               : loadingMode.otaInstallProgress) / 100
                        height: parent.height
                        color: Qt.rgba(1, 1, 1, 0.6)
                        radius: 1
                        Behavior on width {
                            NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                        }
                    }
                }

                // Version
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: themeStore.fontBody
                    color: Qt.rgba(1, 1, 1, 0.5)
                    visible: loadingMode.otaVersion !== ""
                    text: loadingMode.otaVersion
                }
            }
        }
    }

    // --- Connection info mode ---
    Item {
        id: connectionInfoMode
        anchors.fill: parent
        visible: showConnectionInfo
        clip: true

        ColumnLayout {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: "Trying to connect to vehicle system..."
                color: "white"
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.preferredHeight: 16 }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.24)
            }

            Item { Layout.preferredHeight: 16 }

            Text {
                Layout.fillWidth: true
                text: "This usually indicates a missing or unreliable connection between " +
                      "the dashboard computer (DBC) and the middle driver board (MDB).\n\n" +
                      "Check the USB cable if this persists."
                color: Qt.rgba(1, 1, 1, 0.70)
                font.pixelSize: themeStore.fontBody
                lineHeight: 1.4
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.preferredHeight: 16 }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.24)
            }

            Item { Layout.preferredHeight: 16 }

            Text {
                Layout.fillWidth: true
                text: "To put your scooter into drive mode anyway, raise the kickstand, " +
                      "hold both brakes and press the seatbox button."
                color: Qt.rgba(1, 1, 1, 0.60)
                font.pixelSize: themeStore.fontBody
                lineHeight: 1.4
                lineHeightMode: Text.ProportionalHeight
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
        font.pixelSize: themeStore.fontBody
        visible: maintenanceScreen.stateRaw.length > 0
    }
}
