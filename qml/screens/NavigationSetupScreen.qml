import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: navSetupScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#0090B8"
    readonly property color checkColor: "#4CAF50"
    readonly property color crossColor: "#F44336"
    readonly property color doneColor: "#4CAF50"
    readonly property color errorColor: "#F44336"

    readonly property bool mapsOk: typeof navAvailabilityService !== "undefined"
                                    ? navAvailabilityService.localDisplayMapsAvailable : false
    readonly property bool routingOk: typeof navAvailabilityService !== "undefined"
                                       ? navAvailabilityService.routingAvailable : false

    // Setup mode: 0=DisplayMaps, 1=Routing, 2=Both
    readonly property int mode: typeof screenStore !== "undefined" ? screenStore.setupMode : 2

    // Download service bindings
    readonly property bool hasDownloadService: typeof mapDownloadService !== "undefined" && mapDownloadService !== null
    readonly property int dlStatus: hasDownloadService ? mapDownloadService.status : 0
    readonly property double dlProgress: hasDownloadService ? mapDownloadService.progress : 0
    readonly property string dlRegion: hasDownloadService ? mapDownloadService.regionName : ""
    readonly property string dlError: hasDownloadService ? mapDownloadService.errorMessage : ""
    readonly property bool dlUpdateAvailable: hasDownloadService ? mapDownloadService.updateAvailable : false
    readonly property real dlDownloaded: hasDownloadService ? mapDownloadService.downloadedBytes : 0
    readonly property real dlTotal: hasDownloadService ? mapDownloadService.totalBytes : 0

    // Status enum values (matching MapDownloadStatus)
    readonly property int statusIdle: 0
    readonly property int statusCheckingUpdates: 1
    readonly property int statusLocating: 2
    readonly property int statusDownloading: 3
    readonly property int statusInstalling: 4
    readonly property int statusDone: 5
    readonly property int statusError: 6

    // Connectivity
    readonly property bool isOnline: typeof internetStore !== "undefined"
                                      ? internetStore.modemState === 2 : false // ModemState::Connected
    readonly property bool hasGps: typeof gpsStore !== "undefined"
                                    ? gpsStore.gpsState === 2 : false // GpsState::FixEstablished
    readonly property double gpsLat: typeof gpsStore !== "undefined" ? gpsStore.latitude : 0
    readonly property double gpsLng: typeof gpsStore !== "undefined" ? gpsStore.longitude : 0

    // Download button logic
    readonly property bool showDisplayRow: mode === 0 || mode === 2
    readonly property bool showRoutingRow: mode === 1 || mode === 2

    readonly property bool hasRelevantPartial: {
        if (!hasDownloadService) return false
        if (mode === 0) return mapDownloadService.hasPartialDisplayDownload
        if (mode === 1) return mapDownloadService.hasPartialRoutingDownload
        return mapDownloadService.hasPartialDisplayDownload || mapDownloadService.hasPartialRoutingDownload
    }

    readonly property bool canDownload: dlStatus === statusIdle && isOnline && hasGps
    readonly property string downloadButtonLabel: {
        if (dlUpdateAvailable)
            return typeof translations !== "undefined" ? translations.navSetupUpdateButton : "Update"
        if (hasRelevantPartial)
            return typeof translations !== "undefined" ? translations.navSetupResumeButton : "Resume"
        return typeof translations !== "undefined" ? translations.navSetupDownloadButton : "Download"
    }

    // Title logic
    readonly property string titleText: {
        if (typeof translations === "undefined") return "Navigation Setup"
        if (mode === 0) return translations.navSetupTitleMapsUnavailable
        if (mode === 1) return translations.navSetupTitleRoutingUnavailable
        if (!mapsOk && !routingOk) return translations.navSetupTitleBothUnavailable
        if (!mapsOk) return translations.navSetupTitleMapsUnavailable
        if (!routingOk) return translations.navSetupTitleRoutingUnavailable
        return translations.navSetupTitle
    }

    // Auto-resolve region when GPS becomes available
    onHasGpsChanged: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            mapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    onIsOnlineChanged: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            mapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    Component.onCompleted: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            mapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    // Input handling
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onRightTap() {
            if (typeof screenStore !== "undefined") {
                screenStore.closeNavigationSetup()
            }
        }
        function onLeftTap() {
            if (canDownload && hasDownloadService) {
                var needsDisplay = (mode === 0 || mode === 2) && !mapsOk
                var needsRouting = (mode === 1 || mode === 2) && !routingOk
                if (!needsDisplay && !needsRouting && dlUpdateAvailable) {
                    // Update: re-download both
                    needsDisplay = mode === 0 || mode === 2
                    needsRouting = mode === 1 || mode === 2
                }
                mapDownloadService.startDownload(gpsLat, gpsLng, needsDisplay, needsRouting)
            }
        }
    }

    // Recheck availability when download completes
    Connections {
        target: hasDownloadService ? mapDownloadService : null
        function onDownloadComplete() {
            if (typeof navAvailabilityService !== "undefined") {
                navAvailabilityService.recheck()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        Item { Layout.fillHeight: true; Layout.maximumHeight: 16 }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: navSetupScreen.titleText
            color: navSetupScreen.textPrimary
            font.pixelSize: 24
            font.bold: true
        }

        Item { Layout.preferredHeight: 16 }

        // Status rows
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            // Local display maps row
            RowLayout {
                visible: navSetupScreen.showDisplayRow
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Text {
                    text: navSetupScreen.mapsOk ? "\ue15a" : "\ue139"
                    font.family: "Material Icons"
                    font.pixelSize: 18
                    color: navSetupScreen.mapsOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                }
                Text {
                    text: typeof translations !== "undefined" ? translations.navSetupLocalDisplayMaps : "Offline display maps"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: 18
                }
            }

            // Routing service row
            RowLayout {
                visible: navSetupScreen.showRoutingRow
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Text {
                    text: navSetupScreen.routingOk ? "\ue15a" : "\ue139"
                    font.family: "Material Icons"
                    font.pixelSize: 18
                    color: navSetupScreen.routingOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                }
                Text {
                    text: typeof translations !== "undefined" ? translations.navSetupRoutingEngine : "Routing engine"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: 18
                }
            }
        }

        Item { Layout.preferredHeight: 12 }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.preferredHeight: 1
            color: isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        Item { Layout.preferredHeight: 12 }

        // Download section (state-dependent)
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            spacing: 6

            // Idle state
            ColumnLayout {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusIdle
                spacing: 4
                Layout.alignment: Qt.AlignHCenter

                // No internet
                Text {
                    visible: !navSetupScreen.isOnline
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined" ? translations.navSetupDownloadNoInternet : "No internet connection"
                    color: navSetupScreen.textSecondary
                    font.pixelSize: 18
                }

                // Waiting for GPS
                Text {
                    visible: navSetupScreen.isOnline && !navSetupScreen.hasGps
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined" ? translations.navSetupDownloadWaitingGps : "Waiting for GPS fix..."
                    color: navSetupScreen.textSecondary
                    font.pixelSize: 18
                }

                // Region resolved - show name with estimated size
                Text {
                    visible: navSetupScreen.dlRegion !== ""
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var total = 0
                        if (navSetupScreen.hasDownloadService) {
                            if (navSetupScreen.showDisplayRow) total += mapDownloadService.estimatedDisplayBytes
                            if (navSetupScreen.showRoutingRow) total += mapDownloadService.estimatedRoutingBytes
                        }
                        var sizeMB = Math.round(total / 1048576)
                        return navSetupScreen.dlRegion + " (" + sizeMB + " MB)"
                    }
                    color: navSetupScreen.accentColor
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            // Checking updates
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusCheckingUpdates
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupCheckingUpdates : "Checking for updates..."
                color: navSetupScreen.textSecondary
                font.pixelSize: 18
            }

            // Locating
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusLocating
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadLocating : "Detecting your region..."
                color: navSetupScreen.textSecondary
                font.pixelSize: 18
            }

            // Downloading - progress bar + bytes
            ColumnLayout {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusDownloading
                spacing: 4
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined"
                          ? translations.navSetupDownloadProgress.arg(Math.round(navSetupScreen.dlProgress * 100))
                          : "Downloading... " + Math.round(navSetupScreen.dlProgress * 100) + "%"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: 18
                }

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 2
                    color: isDark ? Qt.rgba(1,1,1,0.15) : Qt.rgba(0,0,0,0.1)

                    Rectangle {
                        width: parent.width * navSetupScreen.dlProgress
                        height: parent.height
                        radius: 2
                        color: navSetupScreen.accentColor
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined"
                          ? translations.navSetupDownloadProgressBytes
                              .arg(Math.round(navSetupScreen.dlDownloaded / 1048576))
                              .arg(Math.round(navSetupScreen.dlTotal / 1048576))
                          : Math.round(navSetupScreen.dlDownloaded / 1048576) + " / " + Math.round(navSetupScreen.dlTotal / 1048576) + " MB"
                    color: navSetupScreen.textSecondary
                    font.pixelSize: 18
                }
            }

            // Installing
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusInstalling
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadInstalling : "Installing maps..."
                color: navSetupScreen.textSecondary
                font.pixelSize: 18
            }

            // Done
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusDone
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadDone : "Maps installed successfully"
                color: navSetupScreen.doneColor
                font.pixelSize: 18
                font.bold: true
            }

            // Error
            ColumnLayout {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusError
                spacing: 2
                Layout.alignment: Qt.AlignHCenter

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined" ? translations.navSetupDownloadError : "Download failed"
                    color: navSetupScreen.errorColor
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    visible: navSetupScreen.dlError !== ""
                    Layout.alignment: Qt.AlignHCenter
                    text: navSetupScreen.dlError
                    color: navSetupScreen.textSecondary
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.preferredHeight: 10 }

        // Body text (depends on mode)
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.maximumWidth: parent.width - 80
            text: {
                if (typeof translations === "undefined") return ""
                if (mode === 0) return translations.navSetupDisplayMapsBody
                if (mode === 1) return translations.navSetupRoutingBody
                return translations.navSetupNoRoutingBody
            }
            color: navSetupScreen.textSecondary
            font.pixelSize: 18
            lineHeight: 1.4
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: 12 }

        // QR code
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/ScootUI/assets/icons/nav-setup-qr.png"
            sourceSize.width: 110
            sourceSize.height: 110
            width: 110
            height: 110
            visible: status === Image.Ready
        }

        Item { Layout.preferredHeight: 8 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: typeof translations !== "undefined" ? translations.navSetupScanForInstructions : "Scan for setup instructions"
            color: navSetupScreen.textSecondary
            font.pixelSize: 18
        }

        Item { Layout.fillHeight: true }

        // Footer separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        // Control hints
        ControlHints {
            Layout.fillWidth: true
            leftAction: navSetupScreen.canDownload ? navSetupScreen.downloadButtonLabel : ""
            rightAction: typeof translations !== "undefined" ? translations.controlBack : "Back"
        }
    }
}
