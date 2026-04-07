import QtQuick
import QtQuick.Effects

Row {
    id: statusIndicators
    spacing: 6
    layoutDirection: Qt.RightToLeft

    // Theme-aware icon color (matches Flutter's ColorFilter.mode srcIn)
    readonly property color iconColor: typeof themeStore !== "undefined" && !themeStore.isDark
                                        ? "#000000" : "#FFFFFF"

    readonly property int gpsState: typeof gpsStore !== "undefined" ? gpsStore.gpsState : 0
    readonly property bool gpsRecentFix: typeof gpsStore !== "undefined" ? gpsStore.hasRecentFix : false
    readonly property bool gpsHasTimestamp: typeof gpsStore !== "undefined" ? gpsStore.hasTimestamp : false
    readonly property int btStatus: typeof bluetoothStore !== "undefined" ? bluetoothStore.status : 1
    readonly property string btServiceHealth: typeof bluetoothStore !== "undefined" ? bluetoothStore.serviceHealth : ""
    readonly property int modemState: typeof internetStore !== "undefined" ? internetStore.modemState : 0
    readonly property int cloudStatus: typeof internetStore !== "undefined" ? internetStore.unuCloud : 1
    readonly property int signalQuality: typeof internetStore !== "undefined" ? internetStore.signalQuality : 0
    readonly property string accessTech: typeof internetStore !== "undefined" ? internetStore.accessTech : ""
    readonly property int vehicleState: typeof vehicleStore !== "undefined" ? vehicleStore.state : 0
    readonly property bool otaActive: typeof otaStore !== "undefined" ? otaStore.isActive : false
    readonly property string otaDbcStatus: typeof otaStore !== "undefined" ? otaStore.dbcStatus : "idle"
    readonly property int otaDbcDownloadProgress: typeof otaStore !== "undefined" ? otaStore.dbcDownloadProgress : 0
    readonly property int otaDbcInstallProgress: typeof otaStore !== "undefined" ? otaStore.dbcInstallProgress : 0

    // Visibility settings from SettingsStore (values: "always", "active-or-error", "error", "never")
    readonly property string showGpsSetting: typeof settingsStore !== "undefined" ? settingsStore.showGps : "error"
    readonly property string showBtSetting: typeof settingsStore !== "undefined" ? settingsStore.showBluetooth : "active-or-error"
    readonly property string showCloudSetting: typeof settingsStore !== "undefined" ? settingsStore.showCloud : "never"
    readonly property string showInternetSetting: typeof settingsStore !== "undefined" ? settingsStore.showInternet : "never"

    // Active/error state for each indicator (matches Flutter shouldShowIndicator logic)
    readonly property bool gpsIsActive: (gpsState === 0 && gpsRecentFix) || (gpsState === 2 && gpsRecentFix)
    readonly property bool gpsHasError: gpsState === 3
    readonly property bool btIsActive: btStatus === 0
    readonly property bool btHasError: btServiceHealth === "error"
    readonly property bool cloudIsActive: cloudStatus === 0
    readonly property bool cloudHasError: cloudStatus === 1
    readonly property bool internetIsActive: modemState === 2

    function shouldShowIndicator(setting, isActive, hasError) {
        switch (setting) {
            case "always": return true
            case "active-or-error": return isActive || hasError
            case "error": return hasError
            case "never": return false
            default: return true
        }
    }

    function accessTechLabel(tech) {
        switch (tech) {
            case "5G": return "5G"
            case "LTE":
            case "4G": return "4G"
            case "3G":
            case "UMTS":
            case "HSPA":
            case "HSDPA":
            case "HSUPA": return "3G"
            case "2G":
            case "EDGE":
            case "GPRS": return "2G"
            case "GSM": return "G"
            default: return ""
        }
    }

    // Internet/modem icon with access tech overlay (rightmost in RTL)
    Item {
        width: 24; height: 24
        visible: shouldShowIndicator(showInternetSetting, internetIsActive, false)

        Image {
            id: modemIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            visible: false
            source: {
                if (modemState === 0) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-off.svg"
                if (modemState === 1) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-disconnected.svg"
                // Connected - show signal bars
                var bars = Math.min(Math.floor(signalQuality / 20), 4)
                return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-connected-" + bars + ".svg"
            }
        }
        MultiEffect {
            source: modemIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 1
            font.pixelSize: themeStore.fontMicro
            font.weight: Font.Bold
            color: statusIndicators.iconColor
            visible: modemState >= 2 && accessTech !== ""
            text: accessTechLabel(accessTech)
        }
    }

    // Cloud status icon
    Item {
        width: 24; height: 24
        visible: shouldShowIndicator(showCloudSetting, cloudIsActive, cloudHasError)

        Image {
            id: cloudIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            visible: false
            source: cloudStatus === 0
                ? "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-connected.svg"
                : "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-disconnected.svg"
        }
        MultiEffect {
            source: cloudIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }
    }

    // Bluetooth icon
    Item {
        width: 24; height: 24
        visible: shouldShowIndicator(showBtSetting, btIsActive, btHasError)

        Image {
            id: btIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            visible: false
            source: btStatus === 0
                ? "qrc:/ScootUI/assets/icons/librescoot-bluetooth-connected.svg"
                : "qrc:/ScootUI/assets/icons/librescoot-bluetooth-disconnected.svg"
        }
        MultiEffect {
            source: btIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }
    }

    // GPS icon with pulsing center dot animation when searching
    Item {
        id: gpsItem
        width: 24; height: 24
        visible: shouldShowIndicator(showGpsSetting, gpsIsActive, gpsHasError)

        readonly property bool isSearching: {
            if (gpsState === 0) return !gpsRecentFix && gpsHasTimestamp
            return gpsState === 1 || (gpsState === 2 && !gpsRecentFix)
        }

        readonly property string gpsIconSource: {
            if (gpsState === 0) {
                if (gpsRecentFix) return "qrc:/ScootUI/assets/icons/librescoot-gps-fix-established.svg"
                if (gpsHasTimestamp) return "qrc:/ScootUI/assets/icons/librescoot-gps-searching.svg"
                return "qrc:/ScootUI/assets/icons/librescoot-gps-off.svg"
            }
            switch (gpsState) {
                case 1: return "qrc:/ScootUI/assets/icons/librescoot-gps-searching.svg"
                case 2: return "qrc:/ScootUI/assets/icons/librescoot-gps-fix-established.svg"
                case 3: return "qrc:/ScootUI/assets/icons/librescoot-gps-error.svg"
                default: return "qrc:/ScootUI/assets/icons/librescoot-gps-off.svg"
            }
        }

        // Base GPS icon (always visible)
        Image {
            id: gpsIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            visible: false
            source: gpsItem.isSearching
                ? "qrc:/ScootUI/assets/icons/librescoot-gps-searching.svg"
                : gpsItem.gpsIconSource
        }
        MultiEffect {
            source: gpsIcon
            anchors.fill: parent
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }

        // Pulsing center dot overlay (only when searching)
        Image {
            id: gpsCenterDot
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            visible: false
            source: "qrc:/ScootUI/assets/icons/librescoot-gps-center-dot.svg"
        }
        MultiEffect {
            id: gpsCenterDotEffect
            source: gpsCenterDot
            anchors.fill: parent
            visible: gpsItem.isSearching
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
            opacity: pulseAnimation.running ? pulseAnimation.pulseValue : 0
        }

        SequentialAnimation {
            id: pulseAnimation
            running: gpsItem.isSearching
            loops: Animation.Infinite

            property real pulseValue: 0

            NumberAnimation {
                target: pulseAnimation; property: "pulseValue"
                from: 0.0; to: 1.0; duration: 250
                easing.type: Easing.InOutExpo
            }
            NumberAnimation {
                target: pulseAnimation; property: "pulseValue"
                from: 1.0; to: 0.0; duration: 250
                easing.type: Easing.InOutExpo
            }
            PauseAnimation { duration: 228 }
        }
    }

    // OTA status indicator (leftmost in RTL = last item)
    Row {
        spacing: 2
        visible: otaActive && (vehicleState === 2 || vehicleState === 4)
        layoutDirection: Qt.LeftToRight

        Item {
            width: 24; height: 24

            Image {
                id: otaIcon
                anchors.fill: parent
                sourceSize: Qt.size(24, 24)
                visible: false
                source: {
                    switch (otaDbcStatus) {
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
                            return ""
                    }
                }
            }
            MultiEffect {
                source: otaIcon
                anchors.fill: parent
                colorization: 1.0
                colorizationColor: statusIndicators.iconColor
            }

            // Error overlay (original colors for dark, inverted for light)
            Image {
                anchors.fill: parent
                sourceSize: Qt.size(24, 24)
                source: (typeof themeStore !== "undefined" && themeStore.isDark)
                    ? "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
                    : "qrc:/ScootUI/assets/icons/librescoot-overlay-error-light.svg"
                visible: otaDbcStatus === "error" || otaDbcStatus === "error-failed"
            }
        }

        // Progress text beside icon
        Text {
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.features: {"tnum": 1}
            color: statusIndicators.iconColor
            visible: otaDbcStatus === "downloading" || otaDbcStatus === "preparing" || otaDbcStatus === "installing"
            text: {
                if (otaDbcStatus === "downloading")
                    return otaDbcDownloadProgress
                if (otaDbcStatus === "preparing" || otaDbcStatus === "installing")
                    return otaDbcInstallProgress
                return ""
            }
        }
    }
}
