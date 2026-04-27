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
    // hasValidGps: any non-zero coordinate. The gps.state field flaps to
    // "searching" on transient TPV mode 0/1 while lat/lng stay valid in
    // Redis — gating the menu on FixEstablished surfaced "Waiting for GPS
    // fix" mid-navigation when we plainly knew where we were.
    readonly property bool hasGps: typeof gpsStore !== "undefined"
                                    ? gpsStore.hasValidGps : false
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

    // What the download button will actually fetch given the current mode and
    // component health. Mode 0/1 force a single component; mode 2 fetches
    // whatever is missing. If both are fine but an update is available, we
    // refresh whichever the mode asks for. Used by both the size label and
    // the download trigger so they can't drift.
    readonly property bool willDownloadDisplay: {
        if (showDisplayRow && !mapsOk) return true
        if (dlUpdateAvailable && showDisplayRow) return true
        return false
    }
    readonly property bool willDownloadRouting: {
        if (showRoutingRow && !routingOk) return true
        if (dlUpdateAvailable && showRoutingRow) return true
        return false
    }
    readonly property bool willDownloadAnything: willDownloadDisplay || willDownloadRouting

    readonly property bool canDownload: dlStatus === statusIdle && isOnline && hasGps
                                         && willDownloadAnything
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

    // Scroll state for brake-lever input.
    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    function triggerDownload() {
        if (!canDownload || !hasDownloadService)
            return
        mapDownloadService.startDownload(gpsLat, gpsLng, willDownloadDisplay, willDownloadRouting)
    }

    function closeSelf() {
        if (typeof screenStore !== "undefined")
            screenStore.closeNavigationSetup()
    }

    // Input handling: left scrolls through content and falls through to Back
    // once there's nothing left to scroll; right is the primary action
    // (Download) or Back as a fallback when no action is available.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (navSetupScreen.canScrollDown) {
                scrollAnim.to = Math.min(flickable.contentY + 100,
                                          flickable.contentHeight - flickable.height)
                scrollAnim.restart()
            } else {
                navSetupScreen.closeSelf()
            }
        }
        function onLeftHold() {
            if (navSetupScreen.canScrollUp) {
                scrollAnim.to = Math.max(flickable.contentY - 100, 0)
                scrollAnim.restart()
            }
        }
        function onRightTap() {
            if (navSetupScreen.canDownload)
                navSetupScreen.triggerDownload()
            else
                navSetupScreen.closeSelf()
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

        Flickable {
            id: flickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: scrollContent.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            NumberAnimation {
                id: scrollAnim
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

        ColumnLayout {
            id: scrollContent
            width: flickable.width
            spacing: 0

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 12 }

        // Top row: title + status rows on the left, QR + scan hint on the
        // right. Matches the Update Mode layout so content screens with QR
        // codes are consistent.
        Item {
            id: topRow
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(topLeft.implicitHeight, topRight.implicitHeight)

            Column {
                id: topLeft
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.right: topRight.left
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    width: parent.width
                    text: navSetupScreen.titleText
                    color: navSetupScreen.textPrimary
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }

                Column {
                    spacing: 6

                    Row {
                        visible: navSetupScreen.showDisplayRow
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: navSetupScreen.mapsOk ? MaterialIcon.iconCheckCircleOutline : MaterialIcon.iconCancel
                            font.family: "Material Icons"
                            font.pixelSize: themeStore.fontBody
                            color: navSetupScreen.mapsOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: typeof translations !== "undefined" ? translations.navSetupLocalDisplayMaps : "Offline display maps"
                            color: navSetupScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                        }
                    }

                    Row {
                        visible: navSetupScreen.showRoutingRow
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: navSetupScreen.routingOk ? MaterialIcon.iconCheckCircleOutline : MaterialIcon.iconCancel
                            font.family: "Material Icons"
                            font.pixelSize: themeStore.fontBody
                            color: navSetupScreen.routingOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: typeof translations !== "undefined" ? translations.navSetupRoutingEngine : "Routing engine"
                            color: navSetupScreen.textPrimary
                            font.pixelSize: themeStore.fontBody
                        }
                    }
                }
            }

            Column {
                id: topRight
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 12
                spacing: 4

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "qrc:/ScootUI/assets/icons/nav-setup-qr.png"
                    sourceSize.width: 110
                    sourceSize.height: 110
                    width: 110
                    height: 110
                    visible: status === Image.Ready
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 110
                    horizontalAlignment: Text.AlignHCenter
                    text: typeof translations !== "undefined" ? translations.navSetupScanForInstructions : "Scan for setup instructions"
                    color: navSetupScreen.textSecondary
                    font.pixelSize: themeStore.fontMicro
                    wrapMode: Text.WordWrap
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
                    font.pixelSize: themeStore.fontBody
                }

                // Waiting for GPS
                Text {
                    visible: navSetupScreen.isOnline && !navSetupScreen.hasGps
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof translations !== "undefined" ? translations.navSetupDownloadWaitingGps : "Waiting for GPS fix..."
                    color: navSetupScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                }

                // Region resolved — name + size of what will actually download.
                // Hidden when nothing needs downloading (the "all set" state
                // shows an informational body instead).
                Text {
                    visible: navSetupScreen.dlRegion !== "" && navSetupScreen.willDownloadAnything
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        var total = 0
                        if (navSetupScreen.hasDownloadService) {
                            if (navSetupScreen.willDownloadDisplay) total += mapDownloadService.estimatedDisplayBytes
                            if (navSetupScreen.willDownloadRouting) total += mapDownloadService.estimatedRoutingBytes
                        }
                        var sizeMB = Math.round(total / 1048576)
                        return navSetupScreen.dlRegion + " (" + sizeMB + " MB)"
                    }
                    color: navSetupScreen.accentColor
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                }
            }

            // Checking updates
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusCheckingUpdates
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupCheckingUpdates : "Checking for updates..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // Locating
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusLocating
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadLocating : "Detecting your region..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
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
                    font.pixelSize: themeStore.fontBody
                }

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: themeStore.radiusBar
                    color: isDark ? Qt.rgba(1,1,1,0.15) : Qt.rgba(0,0,0,0.1)

                    Rectangle {
                        width: parent.width * navSetupScreen.dlProgress
                        height: parent.height
                        radius: themeStore.radiusBar
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
                    font.pixelSize: themeStore.fontBody
                }
            }

            // Installing
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusInstalling
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadInstalling : "Installing maps..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // Done
            Text {
                visible: navSetupScreen.dlStatus === navSetupScreen.statusDone
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadDone : "Maps installed successfully"
                color: navSetupScreen.doneColor
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
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
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Bold
                }
                Text {
                    visible: navSetupScreen.dlError !== ""
                    Layout.alignment: Qt.AlignHCenter
                    text: navSetupScreen.dlError
                    color: navSetupScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.preferredHeight: 10 }

        // Body text — picks the description that matches the actual
        // download. In mode 2 with only one side missing, avoid the "both
        // packs" phrasing. When nothing needs downloading (proactive visit
        // from the Navigation submenu with everything installed), show the
        // "all set" message instead.
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.maximumWidth: parent.width - 80
            text: {
                if (typeof translations === "undefined") return ""
                if (!navSetupScreen.willDownloadAnything)
                    return translations.navSetupAllSet
                if (navSetupScreen.willDownloadDisplay && navSetupScreen.willDownloadRouting)
                    return translations.navSetupNoRoutingBody
                if (navSetupScreen.willDownloadDisplay)
                    return translations.navSetupDisplayMapsBody
                return translations.navSetupRoutingBody
            }
            color: navSetupScreen.textSecondary
            font.pixelSize: themeStore.fontBody
            lineHeight: 1.4
            lineHeightMode: Text.ProportionalHeight
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredWidth: 1; Layout.preferredHeight: 16 }
        }  // scrollContent
        }  // Flickable

        // Footer separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        // Control hints. Left reflects its next effect (Scroll while there
        // is content below, then falls through to Back). Right is the
        // primary action (Download/Update/Resume) when one is available,
        // otherwise Back — but suppressed to empty when the left brake
        // would already say Back (no point repeating it).
        ControlHints {
            Layout.fillWidth: true
            leftAction: navSetupScreen.canScrollDown
                ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                : (typeof translations !== "undefined" ? translations.controlBack : "Back")
            rightAction: {
                if (navSetupScreen.canDownload)
                    return navSetupScreen.downloadButtonLabel
                if (!navSetupScreen.canScrollDown)
                    return ""
                return typeof translations !== "undefined" ? translations.controlBack : "Back"
            }
        }
    }
}
