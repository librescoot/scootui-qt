import QtQuick
import "../widgets/components"

Item {
    id: umsOverlay
    anchors.fill: parent

    property string usbStatus: typeof usbStore !== "undefined" ? usbStore.status : "idle"
    property string usbStep: typeof usbStore !== "undefined" ? usbStore.step : ""
    property int usbProgress: typeof usbStore !== "undefined" ? usbStore.progress : 0
    property string usbDetail: typeof usbStore !== "undefined" ? usbStore.detail : ""

    readonly property string dbcUpdateStatus: typeof otaStore !== "undefined" ? otaStore.dbcStatus : "idle"
    readonly property string mdbUpdateStatus: typeof otaStore !== "undefined" ? otaStore.mdbStatus : "idle"
    readonly property string updateComponent: selectUpdateComponent(dbcUpdateStatus, mdbUpdateStatus)
    readonly property string updateStatus: updateComponent === "mdb" ? mdbUpdateStatus : dbcUpdateStatus
    readonly property string displayUpdateStatus: usbStatus === "rebooting" ? "rebooting" : updateStatus
    readonly property string updateVersion: {
        if (typeof otaStore === "undefined") return ""
        return updateComponent === "mdb" ? otaStore.mdbUpdateVersion : otaStore.dbcUpdateVersion
    }
    readonly property int updateProgress: {
        if (typeof otaStore === "undefined") return 0
        if (updateStatus === "downloading")
            return updateComponent === "mdb" ? otaStore.mdbDownloadProgress : otaStore.dbcDownloadProgress
        return updateComponent === "mdb" ? otaStore.mdbInstallProgress : otaStore.dbcInstallProgress
    }
    readonly property string updateErrorMessage: {
        if (typeof otaStore === "undefined") return ""
        return updateComponent === "mdb" ? otaStore.mdbErrorMessage : otaStore.dbcErrorMessage
    }

    function updateStatusRank(status) {
        switch (status) {
        case "error": return 5
        case "installing": return 4
        case "preparing": return 3
        case "downloading": return 2
        case "pending-reboot": return 1
        default: return 0
        }
    }

    // If both images were imported, show the component that still has the
    // most consequential work left. An error must not be hidden by the other
    // board continuing, and active installation outranks pending reboot.
    function selectUpdateComponent(dbcStatus, mdbStatus) {
        return updateStatusRank(mdbStatus) > updateStatusRank(dbcStatus) ? "mdb" : "dbc"
    }

    function processingStepLabel(step) {
        var tr = typeof translations !== "undefined" ? translations : null
        switch (step) {
        case "settings": return tr ? tr.umsStepSettings : "Applying settings"
        case "wireguard": return tr ? tr.umsStepVpn : "Applying VPN configuration"
        case "radio-gaga": return tr ? tr.umsStepRadio : "Applying radio configuration"
        case "uplink-service": return tr ? tr.umsStepConnectivity : "Applying connectivity configuration"
        case "onboot": return tr ? tr.umsStepStartup : "Applying startup configuration"
        case "updates": return tr ? tr.umsStepUpdates : "Checking system updates"
        case "maps": return tr ? tr.umsStepMaps : "Checking maps"
        case "packages": return tr ? tr.umsStepPackages : "Installing packages"
        case "scripts": return tr ? tr.umsStepScripts : "Running maintenance scripts"
        default: return step
        }
    }

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color bgColor: isDark ? "#000000" : "#FFFFFF"
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    // 90% / 70% / 60% / 50% ramp, mirrored per theme
    readonly property color textStep: isDark ? "#E6FFFFFF" : "#E6000000"
    readonly property color textConnect: isDark ? "#B3FFFFFF" : "#B3000000"
    readonly property color textDetail: isDark ? "#99FFFFFF" : "#99000000"
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
        target: typeof inputHandler !== "undefined" ? inputHandler : null
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
            text: typeof translations !== "undefined" ? translations.umsPreparing : "Preparing Storage"
            font.pixelSize: themeStore.fontTitle
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
                font.pixelSize: themeStore.fontHero
                color: umsOverlay.textPrimary
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.umsActive : "Update Mode"
                    font.pixelSize: themeStore.fontHeading
                    font.weight: Font.Bold
                    color: umsOverlay.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.umsConnect : "Connect to Computer"
                    font.pixelSize: themeStore.fontBody
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
                    radius: themeStore.radiusModal
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
                text: typeof translations !== "undefined" ? translations.umsProcessing : "Processing Files"
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Bold
                color: umsOverlay.textPrimary
            }

            // Current step
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: usbStep !== ""
                width: parent.width
                height: stepText.implicitHeight + 12

                Text {
                    id: stepText
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    width: umsOverlay.width - 64
                    text: umsOverlay.processingStepLabel(usbStep)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Medium
                    color: umsOverlay.textStep
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
                        width: parent.width
                        text: usbDetail
                        visible: usbDetail !== ""
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        font.pixelSize: themeStore.fontBody
                        color: umsOverlay.textDetail
                    }
                }
            }

        }

        // UMS has queued the imported image and now waits for update-service
        // to install it and request a safe reboot. Keep one explicit update
        // view throughout that hand-off instead of exposing the internal
        // "awaiting-reboot" state string.
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus === "awaiting-reboot" || usbStatus === "rebooting"
            spacing: 14

            OtaStatusIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                size: 48
                status: umsOverlay.displayUpdateStatus === "idle" || umsOverlay.displayUpdateStatus === ""
                        ? "downloading" : umsOverlay.displayUpdateStatus
                tintColor: umsOverlay.textPrimary
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    var tr = typeof translations !== "undefined" ? translations : null
                    switch (umsOverlay.displayUpdateStatus) {
                    case "rebooting": return tr ? tr.umsApplyingUpdate : "Applying update"
                    case "pending-reboot": return tr ? tr.umsUpdateReady : "Update ready"
                    case "error": return tr ? tr.umsUpdateFailed : "Update installation failed"
                    default: return tr ? tr.umsInstallingUpdate : "Installing update"
                    }
                }
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Bold
                color: umsOverlay.textPrimary
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: umsOverlay.width - 64
                visible: umsOverlay.displayUpdateStatus !== "error"
                         || umsOverlay.updateErrorMessage !== ""
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: {
                    var tr = typeof translations !== "undefined" ? translations : null
                    switch (umsOverlay.displayUpdateStatus) {
                    case "downloading": return tr ? tr.otaDownloadingUpdates : "Downloading update..."
                    case "preparing": return tr ? tr.otaPreparingUpdate : "Preparing update..."
                    case "installing": return tr ? tr.otaInstallingUpdates : "Installing update..."
                    case "pending-reboot": return tr ? tr.umsWaitingForRestart : "Installation complete. Waiting for a safe restart..."
                    case "rebooting": return tr ? tr.umsRestartingAfterUpdate : "Restarting to apply update..."
                    case "error": return umsOverlay.updateErrorMessage
                    default: return tr ? tr.umsStartingInstallation : "Starting installation..."
                    }
                }
                font.pixelSize: themeStore.fontBody
                color: umsOverlay.textConnect
            }

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: umsOverlay.updateStatus === "downloading"
                         || umsOverlay.updateStatus === "preparing"
                         || umsOverlay.updateStatus === "installing"
                width: Math.min(umsOverlay.width - 96, 240)
                height: 4

                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: umsOverlay.trackColor
                }

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(100, umsOverlay.updateProgress)) / 100
                    height: parent.height
                    radius: 2
                    color: umsOverlay.textPrimary
                    Behavior on width { NumberAnimation { duration: 200 } }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: umsOverlay.updateVersion !== ""
                text: umsOverlay.updateVersion
                font.pixelSize: themeStore.fontBody
                color: umsOverlay.textDetail
            }

        }

        // Unknown future UMS state: retain a readable fallback.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: usbStatus !== "preparing" && usbStatus !== "active"
                     && usbStatus !== "processing" && usbStatus !== "awaiting-reboot"
                     && usbStatus !== "rebooting" && usbStatus !== "idle" && usbStatus !== ""
            text: usbStatus
            font.pixelSize: themeStore.fontTitle
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
        // The 3 s hold, which is the gesture ums-service itself exits on.
        leftHoldLong: typeof translations !== "undefined"
                    ? (usbStatus === "preparing" ? translations.controlCancel : translations.umsHoldExit)
                    : (usbStatus === "preparing" ? "Cancel" : "Exit")
    }
}
