import QtQuick
import QtQuick.Effects
import "../components"

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
    // Connectivity classification from modem-service: gates whether the internet
    // icon is worth showing at all. "" = unknown (treat as hidden).
    readonly property string connectivity: typeof internetStore !== "undefined" ? internetStore.connectivity : ""
    readonly property int cloudStatus: typeof internetStore !== "undefined" ? internetStore.unuCloud : 1
    // Cloud applies only if a cloud client (radio-gaga / uplink-service) has
    // published unu-cloud. Absent -> de-clouded scooter -> icon hidden.
    readonly property bool hasCloud: typeof internetStore !== "undefined" ? internetStore.hasUnuCloud : false
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
    readonly property string showCloudSetting: typeof settingsStore !== "undefined" ? settingsStore.showCloud : "active-or-error"
    readonly property string showInternetSetting: typeof settingsStore !== "undefined" ? settingsStore.showInternet : "active-or-error"

    // Temperature state, hoisted from tempRow so the width model can see it.
    readonly property double temp: typeof scooterStore !== "undefined" ? scooterStore.temperature : 0
    readonly property bool hasTemp: typeof scooterStore !== "undefined" && scooterStore.hasTemperature
    readonly property string tempMode: typeof settingsStore !== "undefined"
                                       ? settingsStore.showTemperature : "warning"
    readonly property bool isFrostWarning: typeof scooterStore !== "undefined" && scooterStore.isFrostWarning
    readonly property bool tempShown: hasTemp && tempMode !== "never"
                                      && (tempMode === "always" || isFrostWarning)
    // U+200A HAIR SPACE between number and degree sign.
    readonly property string tempText: hasTemp ? Math.round(temp) + " °" : ""

    readonly property bool internetWanted: shouldShowIndicator(showInternetSetting, internetIsActive, internetHasError)
    readonly property bool cloudWanted: shouldShowIndicator(showCloudSetting, cloudIsActive, cloudHasError)
    readonly property bool btWanted: shouldShowIndicator(showBtSetting, btIsActive, btHasError)
    readonly property bool gpsWanted: shouldShowIndicator(showGpsSetting, gpsIsActive, gpsHasError)
    readonly property bool otaShown: otaActive && (vehicleState === 2 || vehicleState === 4)
    readonly property bool otaProgressActive: otaDbcStatus === "downloading"
                                              || otaDbcStatus === "preparing"
                                              || otaDbcStatus === "installing"
    readonly property string otaProgressText: {
        if (otaDbcStatus === "downloading") return "" + otaDbcDownloadProgress
        if (otaDbcStatus === "preparing" || otaDbcStatus === "installing") return "" + otaDbcInstallProgress
        return ""
    }

    // --- Width-aware degradation -------------------------------------------
    // degradeLevel is assigned by TopStatusBar from the measured budget:
    //   0 full detail             3 cloud -> overflow chip (unless error)
    //   1 no OTA progress digits  4 GPS -> overflow chip (unless error)
    //   2 BT -> overflow chip     5 temp -> overflow chip (unless frost)
    //     (unless error)
    // Anything shown because of an error state never degrades.
    property int degradeLevel: 0

    readonly property bool showOtaProgress: otaProgressActive && degradeLevel < 1
    readonly property bool btChipped: btWanted && !btHasError && degradeLevel >= 2
    readonly property bool cloudChipped: cloudWanted && !cloudHasError && degradeLevel >= 3
    readonly property bool gpsChipped: gpsWanted && !gpsHasError && degradeLevel >= 4
    readonly property bool tempChipped: tempShown && !isFrostWarning && degradeLevel >= 5
    readonly property int chippedCount: (btChipped ? 1 : 0) + (cloudChipped ? 1 : 0)
                                      + (gpsChipped ? 1 : 0) + (tempChipped ? 1 : 0)

    TextMetrics {
        id: tmOtaProgress
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.features: {"tnum": 1}
        text: statusIndicators.otaProgressText
    }
    TextMetrics {
        id: tmTemp
        font.pixelSize: 14
        font.letterSpacing: -0.5
        font.features: {"tnum": 1}
        text: statusIndicators.tempText
    }

    function widthAtLevel(level) {
        var w = 0
        var n = 0
        function add(itemW) { w += (n > 0 ? spacing : 0) + itemW; n++ }

        if (internetWanted) add(24)
        if (cloudWanted && (cloudHasError || level < 3)) add(24)
        if (btWanted && (btHasError || level < 2)) add(24)
        if (gpsWanted && (gpsHasError || level < 4)) add(24)
        if (otaShown)
            add(24 + (otaProgressActive && level < 1 ? 2 + tmOtaProgress.width : 0))
        if (tempShown && (isFrostWarning || level < 5))
            add((isFrostWarning ? 26 : 0) + tmTemp.width)
        var chips = (btWanted && !btHasError && level >= 2 ? 1 : 0)
                  + (cloudWanted && !cloudHasError && level >= 3 ? 1 : 0)
                  + (gpsWanted && !gpsHasError && level >= 4 ? 1 : 0)
                  + (tempShown && !isFrostWarning && level >= 5 ? 1 : 0)
        if (chips > 0) add(24)
        return w
    }

    readonly property var levelWidths: [
        widthAtLevel(0), widthAtLevel(1), widthAtLevel(2),
        widthAtLevel(3), widthAtLevel(4), widthAtLevel(5)
    ]

    // Active/error state for each indicator (matches Flutter shouldShowIndicator logic)
    readonly property bool gpsIsActive: (gpsState === 0 && gpsRecentFix) || (gpsState === 2 && gpsRecentFix)
    readonly property bool gpsHasError: gpsState === 3
    readonly property bool btIsActive: btStatus === 0
    readonly property bool btHasError: btServiceHealth === "error"
    readonly property bool cloudIsActive: hasCloud && cloudStatus === 0
    readonly property bool cloudHasError: hasCloud && cloudStatus === 1
    // Internet icon gating off the connectivity classification (not raw modem-state):
    //   connected            -> active (show)
    //   disconnected, failed  -> error (show: provisioned-but-down / broken modem)
    //   disabled, no-sim, denied (and unknown) -> neither -> hidden under active-or-error
    readonly property bool internetIsActive: connectivity === "connected"
    readonly property bool internetHasError: connectivity === "disconnected" || connectivity === "failed"

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
            case "HSPA+":
            case "HSPA_PLUS": return "H+"
            case "HSPA":
            case "HSDPA":
            case "HSUPA": return "H"
            case "3G":
            case "UMTS":
            case "EVDO0":
            case "EVDOA":
            case "EVDOB": return "3G"
            case "EDGE": return "E"
            case "GPRS": return "2G"
            case "1XRTT": return "1x"
            case "GSM":
            case "GSM_COMPACT":
            case "POTS": return "G"
            default: return ""
        }
    }

    // Internet/modem icon with access tech overlay (rightmost in RTL)
    Item {
        width: 24; height: 24
        visible: internetWanted

        Image {
            id: modemIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            source: {
                if (modemState === 0) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-off.svg"
                if (modemState === 1) return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-disconnected.svg"
                // Connected - show signal bars
                var bars = Math.min(Math.floor(signalQuality / 20), 4)
                return "qrc:/ScootUI/assets/icons/librescoot-internet-modem-connected-" + bars + ".svg"
            }
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: modemIcon
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
        visible: cloudWanted && !cloudChipped

        Image {
            id: cloudIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            source: cloudStatus === 0
                ? "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-connected.svg"
                : "qrc:/ScootUI/assets/icons/librescoot-internet-cloud-disconnected.svg"
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: cloudIcon
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }
    }

    // Bluetooth icon
    Item {
        width: 24; height: 24
        visible: btWanted && !btChipped

        Image {
            id: btIcon
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            source: btStatus === 0
                ? "qrc:/ScootUI/assets/icons/librescoot-bluetooth-connected.svg"
                : "qrc:/ScootUI/assets/icons/librescoot-bluetooth-disconnected.svg"
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: btIcon
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }
    }

    // GPS icon with pulsing center dot animation when searching
    Item {
        id: gpsItem
        width: 24; height: 24
        visible: gpsWanted && !gpsChipped

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
            source: gpsItem.isSearching
                ? "qrc:/ScootUI/assets/icons/librescoot-gps-searching.svg"
                : gpsItem.gpsIconSource
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: gpsIcon
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
        }

        // Pulsing center dot overlay (only when searching)
        Image {
            id: gpsCenterDot
            anchors.fill: parent
            sourceSize: Qt.size(24, 24)
            source: "qrc:/ScootUI/assets/icons/librescoot-gps-center-dot.svg"
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: gpsCenterDot
            visible: gpsItem.isSearching
            opacity: pulseAnimation.running ? pulseAnimation.pulseValue : 0
            colorization: 1.0
            colorizationColor: statusIndicators.iconColor
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
                source: {
                    switch (otaDbcStatus) {
                        case "downloading":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-downloading.svg"
                        case "preparing":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-preparing.svg"
                        case "installing":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-installing.svg"
                        case "pending-reboot":
                        case "rebooting":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-waiting-for-reboot.svg"
                        case "error":
                        case "error-failed":
                        case "reboot-failed":
                            return "qrc:/ScootUI/assets/icons/librescoot-ota-status-error.svg"
                        default:
                            return ""
                    }
                }
                visible: false
                layer.enabled: true
            }
            MultiEffect {
                anchors.fill: parent
                source: otaIcon
                colorization: 1.0
                colorizationColor: statusIndicators.iconColor
            }
        }

        // Progress text beside icon
        Text {
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.features: {"tnum": 1}
            color: statusIndicators.iconColor
            visible: statusIndicators.showOtaProgress
            text: statusIndicators.otaProgressText
        }
    }

    // Ambient temperature (leftmost in RTL = last item). Severe-cold glyph
    // (snowflake + !) sits on the left of the digits once we drop below the
    // frost-warning threshold (< 5 °C). ScooterStore drives display + cold +
    // frost state off a smoothed (~60 s EMA) value so the row can't flicker
    // from sensor jitter. Visibility gated by show-temperature setting:
    //   always  – row always visible, with snow glyph below 5 °C
    //   warning – row visible only while in frost-warning band (< 5 °C)
    //   never   – never shown.
    Row {
        id: tempRow
        spacing: 2
        layoutDirection: Qt.LeftToRight
        // Match sibling icon height so the value sits on the same baseline
        // as the rest of the status row when no snow glyph is present.
        height: 24

        visible: statusIndicators.tempShown && !statusIndicators.tempChipped

        Item {
            visible: statusIndicators.isFrostWarning
            width: visible ? 24 : 0
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: MaterialIcon.iconSevereCold
                font.family: "Material Icons"
                font.pixelSize: 24
                color: statusIndicators.iconColor
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            // U+200A HAIR SPACE between number and degree sign.
            text: statusIndicators.tempText
            font.pixelSize: 14
            font.letterSpacing: -0.5
            font.features: {"tnum": 1}
            color: statusIndicators.iconColor
        }
    }

    OverflowChip {
        count: statusIndicators.chippedCount
        iconColor: statusIndicators.iconColor
        anchors.verticalCenter: parent.verticalCenter
    }
}
