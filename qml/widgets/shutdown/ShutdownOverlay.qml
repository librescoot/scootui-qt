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

    // OTA status during shutdown (centered, persistent)
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: overlay.opacity > 0.9
                 && typeof otaStore !== "undefined"
                 && otaStore.isActive

        Text {
            text: {
                if (typeof otaStore === "undefined") return ""
                if (otaStore.dbcStatus === "downloading")
                    return "Downloading update " + otaStore.dbcUpdateVersion
                if (otaStore.dbcStatus === "preparing")
                    return "Preparing update " + otaStore.dbcUpdateVersion
                if (otaStore.dbcStatus === "installing")
                    return "Installing update " + otaStore.dbcUpdateVersion
                if (otaStore.dbcStatus === "pending-reboot")
                    return "Update ready, will apply on next start"
                if (otaStore.mdbStatus === "downloading")
                    return "Downloading update " + otaStore.mdbUpdateVersion
                if (otaStore.mdbStatus === "preparing")
                    return "Preparing update " + otaStore.mdbUpdateVersion
                if (otaStore.mdbStatus === "installing")
                    return "Installing update " + otaStore.mdbUpdateVersion
                if (otaStore.mdbStatus === "pending-reboot")
                    return "Update ready, will apply on next start"
                return ""
            }
            color: "white"
            font.pixelSize: 18
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
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
