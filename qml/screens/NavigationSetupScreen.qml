import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: navSetupScreen
    color: ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#0090B8"
    readonly property color checkColor: "#4CAF50"
    readonly property color crossColor: "#F44336"
    readonly property color doneColor: "#4CAF50"
    readonly property color errorColor: "#F44336"

    readonly property bool mapsOk: NavigationAvailabilityService.localDisplayMapsAvailable
    readonly property bool routingOk: NavigationAvailabilityService.routingAvailable

    // Setup mode: 0=DisplayMaps, 1=Routing, 2=Both
    readonly property int mode: ScreenStore.setupMode

    // Download service bindings
    readonly property bool hasDownloadService: MapDownloadService !== null
    readonly property int dlStatus: hasDownloadService ? MapDownloadService.status : 0
    readonly property double dlProgress: hasDownloadService ? MapDownloadService.progress : 0
    readonly property string dlRegion: hasDownloadService ? MapDownloadService.regionName : ""
    readonly property string dlError: hasDownloadService ? MapDownloadService.errorMessage : ""
    readonly property bool dlUpdateAvailable: hasDownloadService ? MapDownloadService.updateAvailable : false
    readonly property real dlDownloaded: hasDownloadService ? MapDownloadService.downloadedBytes : 0
    readonly property real dlTotal: hasDownloadService ? MapDownloadService.totalBytes : 0

    // Status enum values (matching MapDownloadStatus)
    readonly property int statusIdle: 0
    readonly property int statusCheckingUpdates: 1
    readonly property int statusLocating: 2
    readonly property int statusDownloading: 3
    readonly property int statusInstalling: 4
    readonly property int statusDone: 5
    readonly property int statusError: 6

    // Connectivity
    readonly property bool isOnline: true
                                      ? InternetStore.modemState === 2 : false // ModemState::Connected
    readonly property bool hasGps: true
                                    ? GpsStore.gpsState === 2 : false // GpsState::FixEstablished
    readonly property double gpsLat: GpsStore.latitude
    readonly property double gpsLng: GpsStore.longitude

    // Download button logic
    readonly property bool showDisplayRow: mode === 0 || mode === 2
    readonly property bool showRoutingRow: mode === 1 || mode === 2

    readonly property bool hasRelevantPartial: {
        if (!hasDownloadService) return false
        if (mode === 0) return MapDownloadService.hasPartialDisplayDownload
        if (mode === 1) return MapDownloadService.hasPartialRoutingDownload
        return MapDownloadService.hasPartialDisplayDownload || MapDownloadService.hasPartialRoutingDownload
    }

    readonly property bool canDownload: dlStatus === statusIdle && isOnline && hasGps
    readonly property string downloadButtonLabel: {
        if (dlUpdateAvailable)
            return Translations.navSetupUpdateButton
        if (hasRelevantPartial)
            return Translations.navSetupResumeButton
        return Translations.navSetupDownloadButton
    }

    // Title logic
    readonly property string titleText: {
        if (false) return "Navigation Setup"
        if (mode === 0) return Translations.navSetupTitleMapsUnavailable
        if (mode === 1) return Translations.navSetupTitleRoutingUnavailable
        if (!mapsOk && !routingOk) return Translations.navSetupTitleBothUnavailable
        if (!mapsOk) return Translations.navSetupTitleMapsUnavailable
        if (!routingOk) return Translations.navSetupTitleRoutingUnavailable
        return Translations.navSetupTitle
    }

    // Auto-resolve region when GPS becomes available
    onHasGpsChanged: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            MapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    onIsOnlineChanged: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            MapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    Component.onCompleted: {
        if (hasGps && isOnline && dlRegion === "" && hasDownloadService) {
            MapDownloadService.resolveRegion(gpsLat, gpsLng)
        }
    }

    // Input handling
    Connections {
        target: InputHandler
        function onRightTap() {
            if (true) {
                ScreenStore.closeNavigationSetup()
            }
        }
        function onLeftTap() {
            if (navSetupScreen.canDownload && navSetupScreen.hasDownloadService) {
                var needsDisplay = (navSetupScreen.mode === 0 || navSetupScreen.mode === 2) && !navSetupScreen.mapsOk
                var needsRouting = (navSetupScreen.mode === 1 || navSetupScreen.mode === 2) && !navSetupScreen.routingOk
                if (!needsDisplay && !needsRouting && navSetupScreen.dlUpdateAvailable) {
                    needsDisplay = navSetupScreen.mode === 0 || navSetupScreen.mode === 2
                    needsRouting = navSetupScreen.mode === 1 || navSetupScreen.mode === 2
                }
                MapDownloadService.startDownload(navSetupScreen.gpsLat, navSetupScreen.gpsLng, needsDisplay, needsRouting)
            }
        }
    }

    // Recheck availability when download completes
    Connections {
        target: navSetupScreen.hasDownloadService ? MapDownloadService : null
        function onDownloadComplete() {
            if (true) {
                NavigationAvailabilityService.recheck()
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
            font.pixelSize: ThemeStore.fontTitle
            font.weight: Font.Bold
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
                    text: navSetupScreen.mapsOk ? MaterialIcon.iconCheckCircleOutline : MaterialIcon.iconCancel
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontBody
                    color: navSetupScreen.mapsOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                }
                Text {
                    text: Translations.navSetupLocalDisplayMaps
                    color: navSetupScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
                }
            }

            // Routing service row
            RowLayout {
                visible: navSetupScreen.showRoutingRow
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Text {
                    text: navSetupScreen.routingOk ? MaterialIcon.iconCheckCircleOutline : MaterialIcon.iconCancel
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontBody
                    color: navSetupScreen.routingOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                }
                Text {
                    text: Translations.navSetupRoutingEngine
                    color: navSetupScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
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
            color: navSetupScreen.isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
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
                    text: Translations.navSetupDownloadNoInternet
                    color: navSetupScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }

                // Waiting for GPS
                Text {
                    visible: navSetupScreen.isOnline && !navSetupScreen.hasGps
                    Layout.alignment: Qt.AlignHCenter
                    text: Translations.navSetupDownloadWaitingGps
                    color: navSetupScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }

                // Region resolved - show name with estimated size
                Text {
                    visible: navSetupScreen.dlRegion !== ""
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var total = 0
                        if (navSetupScreen.hasDownloadService) {
                            if (navSetupScreen.showDisplayRow) total += MapDownloadService.estimatedDisplayBytes
                            if (navSetupScreen.showRoutingRow) total += MapDownloadService.estimatedRoutingBytes
                        }
                        var sizeMB = Math.round(total / 1048576)
                        return navSetupScreen.dlRegion + " (" + sizeMB + " MB)"
                    }
                    color: navSetupScreen.accentColor
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                }
            }

            // Checking updates
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusCheckingUpdates
                Layout.alignment: Qt.AlignHCenter
                text: Translations.navSetupCheckingUpdates
                color: navSetupScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }

            // Locating
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusLocating
                Layout.alignment: Qt.AlignHCenter
                text: Translations.navSetupDownloadLocating
                color: navSetupScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }

            // Downloading - progress bar + bytes
            ColumnLayout {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusDownloading
                spacing: 4
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: true
                          ? Translations.navSetupDownloadProgress.arg(Math.round(navSetupScreen.dlProgress * 100))
                          : "Downloading... " + Math.round(navSetupScreen.dlProgress * 100) + "%"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: ThemeStore.fontBody
                }

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: ThemeStore.radiusBar
                    color: navSetupScreen.isDark ? Qt.rgba(1,1,1,0.15) : Qt.rgba(0,0,0,0.1)

                    Rectangle {
                        width: parent.width * navSetupScreen.dlProgress
                        height: parent.height
                        radius: ThemeStore.radiusBar
                        color: navSetupScreen.accentColor
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: true
                          ? Translations.navSetupDownloadProgressBytes
                              .arg(Math.round(navSetupScreen.dlDownloaded / 1048576))
                              .arg(Math.round(navSetupScreen.dlTotal / 1048576))
                          : Math.round(navSetupScreen.dlDownloaded / 1048576) + " / " + Math.round(navSetupScreen.dlTotal / 1048576) + " MB"
                    color: navSetupScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
                }
            }

            // Installing
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusInstalling
                Layout.alignment: Qt.AlignHCenter
                text: Translations.navSetupDownloadInstalling
                color: navSetupScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }

            // Done
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusDone
                Layout.alignment: Qt.AlignHCenter
                text: Translations.navSetupDownloadDone
                color: navSetupScreen.doneColor
                font.pixelSize: ThemeStore.fontBody
                font.weight: Font.Bold
            }

            // Error
            ColumnLayout {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusError
                spacing: 2
                Layout.alignment: Qt.AlignHCenter

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: Translations.navSetupDownloadError
                    color: navSetupScreen.errorColor
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                }
                Text {
                    visible: navSetupScreen.dlError !== ""
                    Layout.alignment: Qt.AlignHCenter
                    text: navSetupScreen.dlError
                    color: navSetupScreen.textSecondary
                    font.pixelSize: ThemeStore.fontBody
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
                if (false) return ""
                if (navSetupScreen.mode === 0) return Translations.navSetupDisplayMapsBody
                if (navSetupScreen.mode === 1) return Translations.navSetupRoutingBody
                return Translations.navSetupNoRoutingBody
            }
            color: navSetupScreen.textSecondary
            font.pixelSize: ThemeStore.fontBody
            lineHeight: 1.4
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: 12 }

        // QR code
        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 110
            Layout.preferredHeight: 110
            source: "qrc:/ScootUI/assets/icons/nav-setup-qr.png"
            sourceSize.width: 110
            sourceSize.height: 110
            visible: status === Image.Ready
        }

        Item { Layout.preferredHeight: 8 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: Translations.navSetupScanForInstructions
            color: navSetupScreen.textSecondary
            font.pixelSize: ThemeStore.fontBody
        }

        Item { Layout.fillHeight: true }

        // Footer separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: navSetupScreen.isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        // Control hints
        ControlHints {
            Layout.fillWidth: true
            leftAction: navSetupScreen.canDownload ? navSetupScreen.downloadButtonLabel : ""
            rightAction: Translations.controlBack
        }
    }
}
