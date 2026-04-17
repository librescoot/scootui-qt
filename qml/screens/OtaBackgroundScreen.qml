import QtQuick
import ScootUI 1.0

Rectangle {
    id: otaScreen
    color: "black"

    readonly property bool locked: VehicleStore.state === 4

    onLockedChanged: {
        if (locked) {
            backlightTimer.restart()
        } else {
            backlightTimer.stop()
            if (true)
                DashboardStore.setBacklightEnabled(true)
        }
    }

    Timer {
        id: backlightTimer
        interval: 15000
        running: otaScreen.locked
        onTriggered: {
            if (true)
                DashboardStore.setBacklightEnabled(false)
        }
    }

    Component.onDestruction: {
        if (true)
            DashboardStore.setBacklightEnabled(true)
    }

    readonly property string dbcStatus: OtaStore.dbcStatus
    readonly property int downloadProgress: OtaStore.dbcDownloadProgress
    readonly property int installProgress: OtaStore.dbcInstallProgress
    readonly property string updateVersion: OtaStore.dbcUpdateVersion
    readonly property string dbcError: OtaStore.dbcError
    readonly property string dbcErrorMessage: OtaStore.dbcErrorMessage

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
            font.pixelSize: ThemeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.8)
            text: {
                var tr = Translations
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
            font.pixelSize: ThemeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.5)
            visible: otaScreen.updateVersion !== ""
            text: otaScreen.updateVersion
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: otaScreen.width - 64
            font.pixelSize: ThemeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.6)
            visible: otaScreen.dbcStatus !== "idle" && !otaScreen.isError
            text: Translations.otaScooterWillTurnOff
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: otaScreen.width - 64
            font.pixelSize: ThemeStore.fontBody
            color: "#ff5555"
            visible: otaScreen.isError && otaScreen.dbcErrorMessage !== ""
            text: otaScreen.dbcErrorMessage
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
