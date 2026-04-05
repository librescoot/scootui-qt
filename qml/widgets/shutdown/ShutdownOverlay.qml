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
            font.pixelSize: 18
            font.bold: true
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
                var status = otaStore.dbcStatus !== "idle" ? otaStore.dbcStatus : otaStore.mdbStatus
                var version = otaStore.dbcStatus !== "idle" ? otaStore.dbcUpdateVersion : otaStore.mdbUpdateVersion
                var versionSuffix = version ? (" " + version) : ""
                switch (status) {
                    case "downloading": return "Downloading update" + versionSuffix
                    case "preparing": return "Preparing update" + versionSuffix
                    case "installing": return "Installing update" + versionSuffix
                    case "pending-reboot": return "Update ready"
                    default: return "Updating..."
                }
            }
            color: "white"
            font.pixelSize: 18
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: "Your scooter will turn off when done.\nYou can unlock it again at any point."
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            visible: {
                if (typeof otaStore === "undefined") return false
                var status = otaStore.dbcStatus !== "idle" ? otaStore.dbcStatus : otaStore.mdbStatus
                return status !== "pending-reboot" && status !== "idle"
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
