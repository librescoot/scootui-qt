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
    Column {
        anchors.centerIn: parent
        spacing: 8
        visible: overlay.opacity > 0.9
                 && typeof otaStore !== "undefined"
                 && otaStore.isActive

        Text {
            text: {
                if (typeof otaStore === "undefined") return ""
                var tr = typeof translations !== "undefined" ? translations : null
                var status = otaStore.dbcStatus !== "idle" ? otaStore.dbcStatus : otaStore.mdbStatus
                var version = otaStore.dbcStatus !== "idle" ? otaStore.dbcUpdateVersion : otaStore.mdbUpdateVersion
                var versionSuffix = version ? (" " + version) : ""
                switch (status) {
                    case "downloading": return (tr ? tr.otaDownloadingUpdates : "Downloading update...") + versionSuffix
                    case "preparing": return (tr ? tr.otaPreparingUpdate : "Preparing update...") + versionSuffix
                    case "installing": return (tr ? tr.otaInstallingUpdates : "Installing update...") + versionSuffix
                    case "pending-reboot": return tr ? tr.otaPendingReboot : "Update installed, will apply next time the scooter is started"
                    default: return tr ? tr.otaInitializing : "Updating..."
                }
            }
            color: "white"
            font.pixelSize: themeStore.fontBody
            font.weight: Font.Bold
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: {
                var tr = typeof translations !== "undefined" ? translations : null
                return tr ? tr.otaScooterWillTurnOff : "Your scooter will turn off when done.\nYou can unlock it again at any point."
            }
            color: Qt.rgba(1, 1, 1, 0.6)
            font.pixelSize: themeStore.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: 280
            anchors.horizontalCenter: parent.horizontalCenter
            visible: {
                if (typeof otaStore === "undefined") return false
                var status = otaStore.dbcStatus !== "idle" ? otaStore.dbcStatus : otaStore.mdbStatus
                return status !== "idle" && status !== "error" && status !== "error-failed"
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
