import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
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

    // Download service bindings
    readonly property bool hasDownloadService: typeof mapDownloadService !== "undefined" && mapDownloadService !== null
    readonly property int dlStatus: hasDownloadService ? mapDownloadService.status : Scooter.MapDownloadStatus.Idle
    readonly property double dlProgress: hasDownloadService ? mapDownloadService.progress : 0
    readonly property string dlRegion: hasDownloadService ? mapDownloadService.regionName : ""
    readonly property string dlError: hasDownloadService ? mapDownloadService.errorMessage : ""
    readonly property bool dlUpdateAvailable: hasDownloadService ? mapDownloadService.updateAvailable : false
    readonly property real dlDownloaded: hasDownloadService ? mapDownloadService.downloadedBytes : 0
    readonly property real dlTotal: hasDownloadService ? mapDownloadService.totalBytes : 0

    // Connectivity
    readonly property bool isOnline: typeof internetStore !== "undefined"
                                      ? internetStore.modemState === Scooter.ModemState.Connected : false
    // hasValidGps: any non-zero coordinate. The gps.state field flaps to
    // "searching" on transient TPV mode 0/1 while lat/lng stay valid in
    // Redis — gating the menu on FixEstablished surfaced "Waiting for GPS
    // fix" mid-navigation when we plainly knew where we were.
    readonly property bool hasGps: typeof gpsStore !== "undefined"
                                    ? gpsStore.hasValidGps : false

    // Download policy comes from MapSetupController (MapSetupPolicy.h); this
    // screen only maps its enums to translated strings.
    readonly property bool hasSetup: typeof mapSetup !== "undefined" && mapSetup !== null
    readonly property bool showDisplayRow: hasSetup && mapSetup.showDisplayRow
    readonly property bool showRoutingRow: hasSetup && mapSetup.showRoutingRow
    readonly property bool willDownloadAnything: hasSetup && mapSetup.willDownloadAnything
    readonly property bool canDownload: hasSetup && mapSetup.canDownload

    readonly property string downloadButtonLabel: {
        var tr = typeof translations !== "undefined" ? translations : null
        if (!hasSetup) return tr ? tr.navSetupDownloadButton : "Download"
        switch (mapSetup.buttonAction) {
        case MapSetupController.Update: return tr ? tr.navSetupUpdateButton : "Update"
        case MapSetupController.Resume: return tr ? tr.navSetupResumeButton : "Resume"
        default: return tr ? tr.navSetupDownloadButton : "Download"
        }
    }

    readonly property string titleText: {
        if (typeof translations === "undefined" || !hasSetup) return "Navigation Setup"
        switch (mapSetup.title) {
        case MapSetupController.MapsUnavailable: return translations.navSetupTitleMapsUnavailable
        case MapSetupController.RoutingUnavailable: return translations.navSetupTitleRoutingUnavailable
        case MapSetupController.BothUnavailable: return translations.navSetupTitleBothUnavailable
        default: return translations.navSetupTitle
        }
    }

    // Scroll state for brake-lever input.
    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    function closeSelf() {
        if (typeof navigator !== "undefined")
            navigator.closeNavigationSetup()
        if (typeof menuController !== "undefined")
            menuController.resume()
    }

    // Left scrolls and goes back; right is the primary action (Download) and
    // scrolls back up. Right tap no longer doubles as Back: it used to close
    // the screen while the bar showed nothing at all for it.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (!navSetupScreen.canScrollDown) return
            scrollAnim.to = Math.min(flickable.contentY + 100,
                                      flickable.contentHeight - flickable.height)
            scrollAnim.restart()
        }
        function onLeftHold() { navSetupScreen.closeSelf() }
        function onRightTap() {
            if (navSetupScreen.hasSetup)
                mapSetup.triggerDownload()
        }
        function onRightHold() {
            if (!navSetupScreen.canScrollUp) return
            scrollAnim.to = Math.max(flickable.contentY - 100, 0)
            scrollAnim.restart()
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
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Idle
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
                        var total = navSetupScreen.hasSetup ? mapSetup.estimatedDownloadBytes : 0
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
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.CheckingUpdates
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupCheckingUpdates : "Checking for updates..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // Locating
            Text {
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Locating
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadLocating : "Detecting your region..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // Downloading - progress bar + bytes
            ColumnLayout {
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Downloading
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
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Installing
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadInstalling : "Installing maps..."
                color: navSetupScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            // Done
            Text {
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Done
                Layout.alignment: Qt.AlignHCenter
                text: typeof translations !== "undefined" ? translations.navSetupDownloadDone : "Maps installed successfully"
                color: navSetupScreen.doneColor
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
            }

            // Error
            ColumnLayout {
                visible: navSetupScreen.dlStatus === Scooter.MapDownloadStatus.Error
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
                if (typeof translations === "undefined" || !navSetupScreen.hasSetup) return ""
                switch (mapSetup.body) {
                case MapSetupController.AllSet: return translations.navSetupAllSet
                case MapSetupController.Both: return translations.navSetupNoRoutingBody
                case MapSetupController.DisplayOnly: return translations.navSetupDisplayMapsBody
                default: return translations.navSetupRoutingBody
                }
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

        // Left scrolls while there is content below it, and always goes back
        // on the hold. Right carries the primary action (Download/Update/
        // Resume) when there is one, and scrolls back up on the hold.
        ControlHints {
            Layout.fillWidth: true
            // The scroll hints come and go with the scroll position; pin the
            // height so the content above does not shift while scrolling.
            reservedRows: 2
            leftTap: navSetupScreen.canScrollDown
                ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                : ""
            leftHold: typeof translations !== "undefined"
                      ? translations.controlBack : "Back"
            rightTap: navSetupScreen.canDownload ? navSetupScreen.downloadButtonLabel : ""
            rightHold: navSetupScreen.canScrollUp
                ? (typeof translations !== "undefined" ? translations.controlScrollUp : "Scroll up")
                : ""
        }
    }
}
