import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"
import "../widgets/navigation"
import "../widgets/cluster"
import "../widgets/indicators"
import "../widgets/map"
import ScootUI 1.0

Rectangle {
    id: mapScreen
    color: ThemeStore.backgroundColor

    // Navigation status enum values
    readonly property int statusNavigating: 2
    readonly property int statusArrived: 4

    property int navStatus: NavigationService.status
    property bool hasNav: navStatus === statusNavigating || navStatus === statusArrived

    // GPS state enum values (must match GpsState in C++)
    readonly property int gpsOff: 0
    readonly property int gpsSearching: 1
    readonly property int gpsFixEstablished: 2
    readonly property int gpsError: 3

    property int currentGpsState: GpsStore.gpsState
    property bool hasGpsFix: currentGpsState === gpsFixEstablished
    property bool mapReady: MapService.isReady

    // Show GPS waiting screen when no fix and map not ready (Flutter: MapOffline + !fixEstablished)
    property bool showWaitingForGps: !hasGpsFix && !mapReady

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

            Column {
                anchors.centerIn: parent
                spacing: 16

                // gps_not_fixed icon (Flutter: Icons.gps_not_fixed, size: 48, color: fgDim)
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: MaterialIcon.iconGpsNotFixed
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontXL
                    color: ThemeStore.isDark
                           ? "#99FFFFFF" : "#8A000000"  // white60 / black54
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.mapWaitingForGps
                    font.pixelSize: ThemeStore.fontBody
                    color: ThemeStore.isDark
                           ? "#FFFFFF" : "#000000"
                    horizontalAlignment: Text.AlignHCenter
                }

                Grid {
                    id: gpsInfoGrid
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    visible: GpsStore.hasTimestamp

                    readonly property color labelColor: ThemeStore.isDark
                                                       ? "#99FFFFFF" : "#8A000000"
                    readonly property color valueColor: ThemeStore.isDark
                                                       ? "#FFFFFF" : "#000000"
                    readonly property int labelSize: ThemeStore.fontCaption
                    readonly property int valueSize: ThemeStore.fontCaption

                    component InfoLabel : Text {
                        font.pixelSize: ThemeStore.fontCaption
                        color: ThemeStore.isDark ? "#99FFFFFF" : "#8A000000"
                        horizontalAlignment: Text.AlignRight
                    }
                    component InfoValue : Text {
                        font.pixelSize: ThemeStore.fontCaption
                        font.family: "monospace"
                        color: ThemeStore.isDark ? "#FFFFFF" : "#000000"
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

            // Vehicle marker at fixed screen position, tilted to match 3D map
            VehicleMarker {
                id: vehicleMarkerItem
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height / 2 + (MapService.vehicleOffsetY) - height / 2
                visible: MapService.isReady
                transform: Rotation {
                    origin.x: vehicleMarkerItem.width / 2
                    origin.y: vehicleMarkerItem.height / 2
                    axis { x: 1; y: 0; z: 0 }
                    angle: 55
                }
            }

            // Blinker icons (icon mode) — overlaid on map, below turn-by-turn
            BlinkerRow {
                anchors.top: parent.top
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
                color: ThemeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
                border.width: 1.5
                border.color: Qt.rgba(1, 0.647, 0, 0.6)  // orange with 60% opacity
                visible: MapService.isOutOfCoverage
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
                        text: Translations.mapOutOfCoverage
                        font.pixelSize: ThemeStore.fontBody
                        font.weight: Font.Medium
                        color: "#FF9800"  // Colors.orange
                    }
                }
            }

            // No-map message (shown when not navigating and no map position)
            Text {
                anchors.centerIn: parent
                visible: !mapScreen.hasNav && (false || !MapService.isReady)
                text: true
                      ? (Translations.navSetDestination)
                      : (Translations.navUnavailable)
                color: ThemeStore.isDark
                       ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(0, 0, 0, 0.4)
                font.pixelSize: ThemeStore.fontBody
                horizontalAlignment: Text.AlignHCenter
            }

            // Turn-by-turn widget (top, full width)
            TurnByTurnWidget {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
            }

            // Navigation status overlay (calculating, rerouting, arrived, error)
            NavigationStatusOverlay {}

            // Road name (bottom center, German road sign styling)
            RoadNameDisplay {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                fontSize: 14
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
                color: ThemeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
                border.width: 1
                border.color: ThemeStore.isDark
                              ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
                visible: opacity > 0
                z: 10

                property bool shouldShow: MapDownloadService.updateAvailable
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
                        text: Translations.mapUpdateBadge
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: ThemeStore.isDark
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
                visible: MapService.isReady
            }

            // Scale bar (bottom-right)
            ScaleBar {
                id: scaleBar
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                visible: MapService.isReady
            }

            // Warning telltales (bottom left) - engine warning, hazards, parking brake
            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 8
                spacing: 8
                visible: showWarnings

                property bool showWarnings: {
                    return VehicleStore.isUnableToDrive === 0  // Toggle::On = 0
                           || (ConnectionStore.usingBackupConnection)
                           || VehicleStore.blinkerState === 3
                           || VehicleStore.state === 4  // Parked
                }

                Rectangle {
                    width: warningRow.width + 16
                    height: warningRow.height + 16
                    radius: ThemeStore.radiusCard
                    color: ThemeStore.isDark
                           ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9)
                    border.width: 1
                    border.color: ThemeStore.isDark
                                  ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.12)

                    Row {
                        id: warningRow
                        anchors.centerIn: parent
                        spacing: 8

                        // Engine warning
                        IndicatorLight {
                            visible: (VehicleStore.isUnableToDrive === 0)
                                     || (ConnectionStore.usingBackupConnection)
                            source: "qrc:/ScootUI/assets/icons/librescoot-engine-warning.svg"
                            active: true
                            blinking: false
                            tintColor: "#FFC107"
                            width: 32; height: 32
                        }

                        // Hazards
                        IndicatorLight {
                            visible: VehicleStore.blinkerState === 3
                            source: "qrc:/ScootUI/assets/icons/librescoot-hazards.svg"
                            active: true
                            blinking: true
                            blinkSource: VehicleStore.blinkOpacity
                            tintColor: "#F44336"
                            width: 32; height: 32
                        }

                        // Parking brake
                        IndicatorLight {
                            visible: VehicleStore.state === 4
                            source: "qrc:/ScootUI/assets/icons/librescoot-parking-brake.svg"
                            active: true
                            blinking: false
                            tintColor: "#F44336"
                            width: 32; height: 32
                        }
                    }
                }
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
