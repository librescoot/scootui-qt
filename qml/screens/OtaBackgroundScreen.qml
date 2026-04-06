import QtQuick
import ScootUI

Rectangle {
    id: otaScreen
    color: "black"

    readonly property bool locked: VehicleStore.state === 4

    onLockedChanged: {
        if (locked) {
            backlightTimer.restart()
        } else {
            backlightTimer.stop()
            OtaStore.setBacklightOff(false)
        }
    }

    Timer {
        id: backlightTimer
        interval: 30000
        running: otaScreen.locked
        onTriggered: {
            OtaStore.setBacklightOff(true)
        }
    }

    Component.onDestruction: {
        OtaStore.setBacklightOff(false)
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

    Column {
        anchors.centerIn: parent
        spacing: 16

        // OTA icon
        Item {
            width: 64; height: 64
            anchors.horizontalCenter: parent.horizontalCenter

            Image {
                id: otaMainIcon
                anchors.fill: parent
                sourceSize: Qt.size(64, 64)
                source: {
                    switch (dbcStatus) {
                        case "downloading":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                        case "preparing":
                        case "installing":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-installing.svg"
                        case "pending-reboot":
                        case "rebooting":
                        case "reboot-failed":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-waiting-for-reboot.svg"
                        case "error":
                        case "error-failed":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                        default:
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                    }
                }
            }

            Image {
                anchors.fill: parent
                sourceSize: Qt.size(64, 64)
                source: "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
                visible: dbcStatus === "error" || dbcStatus === "error-failed"
            }
        }

        // Status text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: ThemeStore.fontBody
            font.weight: Font.Bold
            color: "white"
            text: {
                var tr = Translations
                switch (dbcStatus) {
                    case "downloading": return tr ? tr.otaDownloadingUpdates : "Downloading update..."
                    case "preparing": return tr ? tr.otaPreparingUpdate : "Preparing update..."
                    case "installing": return tr ? tr.otaInstallingUpdates : "Installing update..."
                    case "pending-reboot": return tr ? tr.otaPendingReboot : "Update ready, will apply next time the scooter is started"
                    case "rebooting": return tr ? tr.otaStatusWaitingForReboot : "Waiting for reboot..."
                    case "reboot-failed": return tr ? tr.otaRebootFailed : "Reboot failed"
                    case "error":
                    case "error-failed": return tr ? tr.otaUpdateError : "Update failed"
                    default: return tr ? tr.otaInitializing : "Updating..."
                }
            }
        }

        // Progress bar
        Item {
            width: 200; height: 4
            anchors.horizontalCenter: parent.horizontalCenter
            visible: dbcStatus === "downloading" || dbcStatus === "preparing" || dbcStatus === "installing"

            Rectangle {
                anchors.fill: parent
                color: "#333333"
                radius: ThemeStore.radiusBar
            }

            Rectangle {
                width: parent.width * (currentProgress / 100)
                height: parent.height
                color: "#2196F3"
                radius: ThemeStore.radiusBar

                Behavior on width {
                    NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                }
            }

            // Progress percentage text
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: 8
                font.pixelSize: ThemeStore.fontBody
                color: "white"
                text: currentProgress + "%"
            }
        }

        // Spacer for progress text below bar
        Item {
            width: 1; height: 16
            visible: dbcStatus === "downloading" || dbcStatus === "preparing" || dbcStatus === "installing"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: ThemeStore.fontBody
            color: "#ff8800"
            visible: dbcStatus === "preparing" || dbcStatus === "installing"
            text: Translations.otaDoNotPowerOff
            horizontalAlignment: Text.AlignHCenter
        }

        // Version text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: ThemeStore.fontBody
            color: "#aaaaaa"
            visible: updateVersion !== ""
            text: "Version: " + updateVersion
        }

        // Error message
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: ThemeStore.fontBody
            color: "#ff5555"
            visible: (dbcStatus === "error" || dbcStatus === "error-failed") && dbcErrorMessage !== ""
            text: dbcErrorMessage
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: 240
        }
    }
}
