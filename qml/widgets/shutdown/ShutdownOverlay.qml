import QtQuick
import "../components"

Item {
    id: shutdownOverlay

    visible: overlay.opacity > 0

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "black"
        opacity: 0
    }

    // Shutdown content text (fades out during animation)
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        visible: shutdownAnim.running

        // Fade out content during first half of shutdown animation
        opacity: shutdownAnim.running ? Math.max(0, 1.0 - (overlay.opacity - 0.8) * 5) : 0

        Text {
            text: "Shutting down..."
            color: "white"
            font.pixelSize: themeStore.fontBody
            font.weight: Font.Bold
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Flickable {
        anchors.centerIn: parent
        width: parent.width - 64
        height: Math.min(contentHeight, parent.height - 64)
        contentHeight: otaContent.height
        clip: true
        visible: overlay.opacity > 0.9 && typeof otaStore !== "undefined"
                 && (otaStore.dbcStatus !== "idle" || otaStore.mdbStatus !== "idle")

        Column {
            id: otaContent
            width: parent.width
            spacing: 20

            Repeater {
                model: ["dbc", "mdb"]
                delegate: Column {
                    id: board
                    required property string modelData
                    readonly property string status: typeof otaStore !== "undefined"
                        ? otaStore[modelData + "Status"] : "idle"
                    readonly property bool failed: status === "error" || status === "error-failed"
                                                    || status === "reboot-failed"
                    width: otaContent.width
                    spacing: 6
                    visible: status !== "idle"

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 10
                        OtaStatusIcon { status: board.status; size: 32 }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: board.modelData.toUpperCase()
                            font.pixelSize: themeStore.fontBody
                            font.bold: true
                            color: "white"
                        }
                    }
                    Text {
                        objectName: "shutdownOtaStatus_" + board.modelData
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: themeStore.fontBody
                        text: {
                            if (board.failed) return translations.otaUpdateError
                            switch (board.status) {
                                case "downloading": return translations.otaDownloadingUpdates
                                case "preparing": return translations.otaPreparingUpdate
                                case "installing": return translations.otaInstallingUpdates
                                case "pending-reboot": return translations.otaPendingReboot
                                default: return translations.otaInitializing
                            }
                        }
                    }
                    Text {
                        objectName: "shutdownOtaVersion_" + board.modelData
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: themeStore.fontBody
                        color: Qt.rgba(1, 1, 1, 0.6)
                        text: typeof otaStore !== "undefined" ? otaStore[board.modelData + "UpdateVersion"] : ""
                        visible: text !== ""
                    }
                    Text {
                        objectName: "shutdownOtaError_" + board.modelData
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        font.pixelSize: themeStore.fontBody
                        color: "#FF9800"
                        text: typeof otaStore !== "undefined" ? otaStore[board.modelData + "ErrorMessage"] : ""
                        visible: board.failed && text !== ""
                    }
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: themeStore.fontBody
                text: translations.otaScooterWillTurnOff
                visible: typeof otaStore !== "undefined" && otaStore.isActive
                         && ![otaStore.dbcStatus, otaStore.mdbStatus].some(function(status) {
                             return status === "error" || status === "error-failed" || status === "reboot-failed"
                         })
            }
        }
    }

    Connections {
        target: typeof shutdownStore !== "undefined" ? shutdownStore : null

        function onShuttingDownChanged() {
            if (shutdownStore.isShuttingDown && !shutdownStore.showBlackout) {
                shutdownAnim.start()
            } else if (!shutdownStore.isShuttingDown) {
                shutdownAnim.stop()
                blackoutAnim.stop()
                overlay.opacity = 0
            }
        }

        function onShowBlackoutChanged() {
            if (shutdownStore.showBlackout) {
                shutdownAnim.stop()
                blackoutAnim.start()
            }
        }
    }

    // Normal shutdown: fade 0.8 -> 1.0 over 1500ms
    NumberAnimation {
        id: shutdownAnim
        target: overlay
        property: "opacity"
        from: 0.8; to: 1.0
        duration: 1500
    }

    // SIGTERM blackout: fast fade 0 -> 1.0 over 600ms
    NumberAnimation {
        id: blackoutAnim
        target: overlay
        property: "opacity"
        to: 1.0
        duration: 600
    }
}
