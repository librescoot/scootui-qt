import QtQuick

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

    // OTA status during shutdown (centered, persistent after fade)
    readonly property string otaActiveStatus: {
        if (typeof otaStore === "undefined") return "idle"
        return otaStore.dbcStatus !== "idle" ? otaStore.dbcStatus : otaStore.mdbStatus
    }
    readonly property string otaActiveVersion: {
        if (typeof otaStore === "undefined") return ""
        return otaStore.dbcStatus !== "idle" ? otaStore.dbcUpdateVersion : otaStore.mdbUpdateVersion
    }

    Column {
        anchors.centerIn: parent
        spacing: 16
        visible: overlay.opacity > 0.9
                 && typeof otaStore !== "undefined"
                 && otaStore.isActive

        OtaStatusIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            status: shutdownOverlay.otaActiveStatus
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: shutdownOverlay.width - 64
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Qt.rgba(1, 1, 1, 0.8)
            font.pixelSize: themeStore.fontBody
            text: {
                var tr = typeof translations !== "undefined" ? translations : null
                switch (shutdownOverlay.otaActiveStatus) {
                    case "downloading": return tr ? tr.otaDownloadingUpdates : "Downloading update..."
                    case "preparing": return tr ? tr.otaPreparingUpdate : "Preparing update..."
                    case "installing": return tr ? tr.otaInstallingUpdates : "Installing update..."
                    case "pending-reboot": return tr ? tr.otaPendingReboot : "Update installed, will apply next time the scooter is started"
                    default: return tr ? tr.otaInitializing : "Updating..."
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: themeStore.fontBody
            color: Qt.rgba(1, 1, 1, 0.5)
            visible: shutdownOverlay.otaActiveVersion !== ""
            text: shutdownOverlay.otaActiveVersion
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: shutdownOverlay.width - 64
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Qt.rgba(1, 1, 1, 0.6)
            font.pixelSize: themeStore.fontBody
            text: typeof translations !== "undefined" ? translations.otaScooterWillTurnOff : "Your scooter will turn off when done.\nYou can unlock it again at any point."
            visible: {
                var s = shutdownOverlay.otaActiveStatus
                return s !== "idle" && s !== "error" && s !== "error-failed"
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
