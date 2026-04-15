import QtQuick

Rectangle {
    id: otaScreen
    color: "black"

    readonly property bool locked: typeof vehicleStore !== "undefined" && vehicleStore.state === 4

    onLockedChanged: {
        if (locked) {
            backlightTimer.restart()
        } else {
            backlightTimer.stop()
            if (typeof dashboardStore !== "undefined")
                dashboardStore.setBacklightEnabled(true)
        }
    }

    Timer {
        id: backlightTimer
        interval: 15000
        running: otaScreen.locked
        onTriggered: {
            if (typeof dashboardStore !== "undefined")
                dashboardStore.setBacklightEnabled(false)
        }
    }

    Component.onDestruction: {
        if (typeof dashboardStore !== "undefined")
            dashboardStore.setBacklightEnabled(true)
    }

    readonly property string dbcStatus: typeof otaStore !== "undefined" ? otaStore.dbcStatus : "idle"
    readonly property int downloadProgress: typeof otaStore !== "undefined" ? otaStore.dbcDownloadProgress : 0
    readonly property int installProgress: typeof otaStore !== "undefined" ? otaStore.dbcInstallProgress : 0
    readonly property string updateVersion: typeof otaStore !== "undefined" ? otaStore.dbcUpdateVersion : ""
    readonly property string dbcError: typeof otaStore !== "undefined" ? otaStore.dbcError : ""
    readonly property string dbcErrorMessage: typeof otaStore !== "undefined" ? otaStore.dbcErrorMessage : ""

    readonly property int currentProgress: {
        if (dbcStatus === "downloading") return downloadProgress
        if (dbcStatus === "preparing") return installProgress
        if (dbcStatus === "installing") return installProgress
        return 0
    }

    readonly property bool hasProgress: dbcStatus === "downloading"
                                        || dbcStatus === "preparing"
                                        || dbcStatus === "installing"
    readonly property bool isError: dbcStatus === "error" || dbcStatus === "error-failed"

    Column {
        anchors.centerIn: parent
        spacing: 16

        OtaStatusIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            status: otaScreen.dbcStatus
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: otaScreen.width - 64
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: themeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.8)
            text: {
                var tr = typeof translations !== "undefined" ? translations : null
                switch (otaScreen.dbcStatus) {
                    case "downloading": return tr ? tr.otaDownloadingUpdates : "Downloading update..."
                    case "preparing": return tr ? tr.otaPreparingUpdate : "Preparing update..."
                    case "installing": return tr ? tr.otaInstallingUpdates : "Installing update..."
                    case "pending-reboot": return tr ? tr.otaPendingReboot : "Update installed, will apply next time the scooter is started"
                    case "rebooting": return tr ? tr.otaStatusWaitingForReboot : "Waiting for reboot..."
                    case "reboot-failed": return tr ? tr.otaRebootFailed : "Reboot failed"
                    case "error":
                    case "error-failed": return tr ? tr.otaUpdateError : "Update failed"
                    default: return tr ? tr.otaInitializing : "Updating..."
                }
            }
        }

        Item {
            width: 160; height: 3
            anchors.horizontalCenter: parent.horizontalCenter
            visible: otaScreen.hasProgress

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(1, 1, 1, 0.2)
                radius: 1
            }

            Rectangle {
                width: parent.width * (otaScreen.currentProgress / 100)
                height: parent.height
                color: Qt.rgba(1, 1, 1, 0.6)
                radius: 1

                Behavior on width {
                    NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: themeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.5)
            visible: otaScreen.updateVersion !== ""
            text: otaScreen.updateVersion
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: otaScreen.width - 64
            font.pixelSize: themeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.6)
            visible: otaScreen.dbcStatus !== "idle" && !otaScreen.isError
            text: typeof translations !== "undefined" ? translations.otaScooterWillTurnOff : "Your scooter will turn off when done.\nYou can unlock it again at any point."
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: otaScreen.width - 64
            font.pixelSize: themeStore.fontBody
            color: "#ff5555"
            visible: otaScreen.isError && otaScreen.dbcErrorMessage !== ""
            text: otaScreen.dbcErrorMessage
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
