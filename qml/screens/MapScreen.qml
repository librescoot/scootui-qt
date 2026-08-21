import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: mapScreen
    color: typeof ThemeStore !== "undefined" ? ThemeStore.backgroundColor : "black"

    // Navigation status enum values
    readonly property int statusNavigating: 2
    readonly property int statusArrived: 4

    property int navStatus: typeof NavigationService !== "undefined"
                            ? NavigationService.status : 0
    property bool hasNav: navStatus === statusNavigating || navStatus === statusArrived

    // GPS state enum values (must match GpsState in C++)
    readonly property int gpsOff: 0
    readonly property int gpsSearching: 1
    readonly property int gpsFixEstablished: 2
    readonly property int gpsError: 3

    property int currentGpsState: typeof GpsStore !== "undefined" ? GpsStore.gpsState : 0
    property bool hasGpsFix: currentGpsState === gpsFixEstablished
    property bool hasRecentFix: typeof GpsStore !== "undefined" ? GpsStore.hasRecentFix : false
    property bool mapReady: typeof MapService !== "undefined" && MapService.isReady
    property bool hasRoute: typeof NavigationService !== "undefined" && NavigationService.hasRoute

    // Full-screen "waiting for GPS" takes over only when there's no position
    // we can do anything useful with: no recent fix AND no route to dead-
    // reckon along. If we have a route, we keep the map and DR along it —
    // the vehicle marker grays out on its own (via hasRecentFix).
    property bool showWaitingForGps: !hasRecentFix && !hasRoute

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top status bar
        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        // GPS waiting state (Flutter: _buildWaitingForGps)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: mapScreen.showWaitingForGps

            // Blinker icons — the map-area BlinkerRow below is hidden along
            // with the map while waiting for a fix, so this takeover view
            // needs its own (also covers hazards, which BlinkerOverlay
            // doesn't render).
            BlinkerRow {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.topMargin: 4
                z: 5
            }

            Column {
                anchors.centerIn: parent
                spacing: 16

                // gps_not_fixed icon (Flutter: Icons.gps_not_fixed, size: 48, color: fgDim)
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: MaterialIcon.iconGpsNotFixed
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontXL
                    color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                           ? "#99FFFFFF" : "#8A000000"  // white60 / black54
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof Translations !== "undefined"
                          ? Translations.mapWaitingForGps : "Waiting for GPS fix"
                    font.pixelSize: ThemeStore.fontBody
                    color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                           ? "#FFFFFF" : "#000000"
                    horizontalAlignment: Text.AlignHCenter
                }

                Grid {
                    id: gpsInfoGrid
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    // Show as soon as the chip is producing NMEA — sats/SNR/DOP
                    // are useful while waiting for a fix. hasTimestamp alone
                    // would keep this hidden on a cold boot until the first
                    // fix; satellitesVisible > 0 covers the search window.
                    visible: typeof GpsStore !== "undefined"
                             && (GpsStore.hasTimestamp || GpsStore.satellitesVisible > 0)

                    readonly property color labelColor: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                                                       ? "#99FFFFFF" : "#8A000000"
                    readonly property color valueColor: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                                                       ? "#FFFFFF" : "#000000"
                    readonly property int labelSize: ThemeStore.fontCaption
                    readonly property int valueSize: ThemeStore.fontCaption

                    component InfoLabel : Text {
                        font.pixelSize: gpsInfoGrid.labelSize
                        color: gpsInfoGrid.labelColor
                        horizontalAlignment: Text.AlignRight
                    }
                    component InfoValue : Text {
                        font.pixelSize: gpsInfoGrid.valueSize
                        font.family: "monospace"
                        color: gpsInfoGrid.valueColor
                    }

                    InfoLabel { text: "Fix" }
                    InfoValue {
                        text: {
                            var f = GpsStore.fix
                            if (!f || f === "none") return "—"
                            return f.toUpperCase()
                        }
                    }

                    InfoLabel { text: "Satellites" }
                    InfoValue {
                        text: GpsStore.satellitesUsed + " / " + GpsStore.satellitesVisible
                    }

                    InfoLabel { text: "SNR" }
                    InfoValue {
                        text: GpsStore.snr > 0 ? GpsStore.snr.toFixed(1) + " dB" : "—"
                    }

                    InfoLabel { text: "Accuracy" }
                    InfoValue {
                        text: GpsStore.eph > 0 ? "±" + GpsStore.eph.toFixed(1) + " m" : "—"
                    }

                    InfoLabel { text: "HDOP / PDOP" }
                    InfoValue {
                        text: (GpsStore.hdop > 0 ? GpsStore.hdop.toFixed(1) : "—")
                              + " / "
                              + (GpsStore.pdop > 0 ? GpsStore.pdop.toFixed(1) : "—")
                    }

                    InfoLabel { text: "Mode" }
                    InfoValue {
                        text: GpsStore.mode || "—"
                    }

                    InfoLabel { text: "Last TTFF" }
                    InfoValue {
                        text: GpsStore.lastTtffSeconds > 0
                              ? GpsStore.lastTtffSeconds.toFixed(0) + " s"
                                + (GpsStore.lastTtffMode ? " (" + GpsStore.lastTtffMode + ")" : "")
                              : "—"
                    }
                }
            }
        }

        // Map area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !mapScreen.showWaitingForGps

            // Map view (QMapLibre wrapper)
            MapViewWidget {
                anchors.fill: parent
            }

            // Confetti layer — renders on top of the map but below widgets (z>=5 below)
            MilestoneConfettiLayer {
                anchors.fill: parent
            }

            // Vehicle marker at fixed screen position, tilted to match 3D map.
            // In the flat 2D top-down view it stays upright (no X-tilt).
            VehicleMarker {
                id: vehicleMarkerItem
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height / 2 + (typeof MapService !== "undefined" ? MapService.vehicleOffsetY : 0) - height / 2
                visible: typeof MapService !== "undefined" && MapService.isReady
                transform: Rotation {
                    origin.x: vehicleMarkerItem.width / 2
                    origin.y: vehicleMarkerItem.height / 2
                    axis { x: 1; y: 0; z: 0 }
                    angle: (typeof SettingsStore !== "undefined" && SettingsStore.mapViewMode === 1) ? 0 : 55
                }
            }

            // Blinker icons (icon mode) — sit just below the turn-by-turn
            // banner when navigating, otherwise hug the top of the map.
            // Without this, the 56 px circles stack on top of the maneuver
            // icon (left) and time-info pill (right) inside the banner.
            BlinkerRow {
                anchors.top: tbtWidget.visible ? tbtWidget.bottom : parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.topMargin: 4
                z: 5
            }

            // Out of coverage overlay (Flutter: _buildOutOfCoverageOverlay)
            // Floating pill at top when GPS is outside mbtiles bounds
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 8
                width: outOfCoverageRow.width + 24  // padding h:12
                height: outOfCoverageRow.height + 16  // padding v:8
                radius: ThemeStore.radiusCard
                color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
                border.width: 1.5
                border.color: Qt.rgba(1, 0.647, 0, 0.6)  // orange with 60% opacity
                visible: typeof MapService !== "undefined" && MapService.isOutOfCoverage
                z: 10

                Row {
                    id: outOfCoverageRow
                    anchors.centerIn: parent
                    spacing: 8

                    // map_outlined icon (Flutter: Icons.map_outlined, color: orange, size: 16)
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: MaterialIcon.iconMap
                        font.family: "Material Icons"
                        font.pixelSize: ThemeStore.fontBody
                        color: "#FF9800"  // Colors.orange
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: typeof Translations !== "undefined"
                              ? Translations.mapOutOfCoverage : "No map data for current location"
                        font.pixelSize: ThemeStore.fontBody
                        font.weight: Font.Medium
                        color: "#FF9800"  // Colors.orange
                    }
                }
            }

            // No-map message (shown when not navigating and no map position)
            Text {
                anchors.centerIn: parent
                visible: !mapScreen.hasNav && (typeof MapService === "undefined" || !MapService.isReady)
                text: typeof NavigationService !== "undefined"
                      ? (typeof Translations !== "undefined" ? Translations.navSetDestination
                         : "Set a destination to start navigation")
                      : (typeof Translations !== "undefined" ? Translations.navUnavailable
                         : "Navigation unavailable")
                color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                       ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(0, 0, 0, 0.4)
                font.pixelSize: ThemeStore.fontBody
                horizontalAlignment: Text.AlignHCenter
            }

            // Turn-by-turn widget (top, full width)
            TurnByTurnWidget {
                id: tbtWidget
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
            }

            // Navigation status overlay (calculating, rerouting, arrived, error)
            NavigationStatusOverlay {}

            // Speed limit + road name (bottom center). Limit sign sits before
            // the road-name pill; either can show on its own.
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                spacing: 4

                SpeedLimitIndicator {
                    iconSize: 36
                    anchors.verticalCenter: parent.verticalCenter
                }

                RoadNameDisplay {
                    anchors.verticalCenter: parent.verticalCenter
                    fontSize: 14
                }
            }

            // Map update indicator (top-left, fades after 20s in ready-to-drive)
            Rectangle {
                id: mapUpdateBadge
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 8
                anchors.topMargin: 8
                width: updateBadgeRow.width + 16
                height: updateBadgeRow.height + 10
                radius: ThemeStore.radiusCard
                color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
                border.width: 1
                border.color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                              ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
                visible: opacity > 0
                z: 10

                property bool shouldShow: typeof MapDownloadService !== "undefined"
                                          && MapDownloadService.updateAvailable
                property bool fadingOut: false

                opacity: shouldShow && !fadingOut ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 1000 } }

                Row {
                    id: updateBadgeRow
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: MaterialIcon.iconUpdate
                        font.family: "Material Icons"
                        font.pixelSize: 16
                        color: "#40C8F0"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: typeof Translations !== "undefined"
                              ? Translations.mapUpdateBadge : "Map update"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                               ? "#FFFFFF" : "#000000"
                    }
                }

                Timer {
                    id: fadeBadgeTimer
                    interval: 20000
                    onTriggered: mapUpdateBadge.fadingOut = true
                }

                Connections {
                    target: VehicleStore
                    function onStateChanged() {
                        if (VehicleStore.state === 2) { // ReadyToDrive
                            if (mapUpdateBadge.shouldShow)
                                fadeBadgeTimer.start()
                        } else {
                            fadeBadgeTimer.stop()
                            mapUpdateBadge.fadingOut = false
                        }
                    }
                }
            }

            // North indicator (bottom-right, fixed position)
            NorthIndicator {
                anchors.right: parent.right
                anchors.bottom: scaleBar.top
                anchors.rightMargin: 8
                anchors.bottomMargin: 4
                visible: typeof MapService !== "undefined" && MapService.isReady
            }

            // Scale bar (bottom-right)
            ScaleBar {
                id: scaleBar
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                visible: typeof MapService !== "undefined" && MapService.isReady
            }

            // Warning telltales (bottom left)
            TelltalePanel {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 8
            }
        }

        // Bottom status bar with speed center widget (matches Flutter layout)
        UnifiedBottomStatusBar {
            id: bottomBar
            Layout.fillWidth: true

            SpeedCenterWidget {}
        }
    }

    readonly property real bottomBarHeight: bottomBar.height
}
