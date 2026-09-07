import QtQuick
import QtQuick.Layouts
import "../notifications"
import "../widgets/status_bars"
import "../widgets/components"
import "../widgets/navigation"
import "../widgets/cluster"
import "../widgets/indicators"
import "../widgets/map"

Rectangle {
    id: mapScreen
    color: typeof themeStore !== "undefined" ? themeStore.backgroundColor : "black"

    // Navigation status enum values
    readonly property int statusNavigating: 2
    readonly property int statusArrived: 4

    property int navStatus: typeof navigationService !== "undefined"
                            ? navigationService.status : 0
    property bool hasNav: navStatus === statusNavigating || navStatus === statusArrived

    // GPS state enum values (must match GpsState in C++)
    readonly property int gpsOff: 0
    readonly property int gpsSearching: 1
    readonly property int gpsFixEstablished: 2
    readonly property int gpsError: 3

    property int currentGpsState: typeof gpsStore !== "undefined" ? gpsStore.gpsState : 0
    property bool hasGpsFix: currentGpsState === gpsFixEstablished
    property bool hasRecentFix: typeof gpsStore !== "undefined" ? gpsStore.hasRecentFix : false
    property bool mapReady: typeof mapService !== "undefined" && mapService.isReady
    property bool hasRoute: typeof navigationService !== "undefined" && navigationService.hasRoute

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
                    font.pixelSize: themeStore.fontXL
                    color: typeof themeStore !== "undefined" && themeStore.isDark
                           ? "#99FFFFFF" : "#8A000000"  // white60 / black54
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.mapWaitingForGps : "Waiting for GPS fix"
                    font.pixelSize: themeStore.fontBody
                    color: typeof themeStore !== "undefined" && themeStore.isDark
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
                    visible: typeof gpsStore !== "undefined"
                             && (gpsStore.hasTimestamp || gpsStore.satellitesVisible > 0)

                    readonly property color labelColor: typeof themeStore !== "undefined" && themeStore.isDark
                                                       ? "#99FFFFFF" : "#8A000000"
                    readonly property color valueColor: typeof themeStore !== "undefined" && themeStore.isDark
                                                       ? "#FFFFFF" : "#000000"
                    readonly property int labelSize: themeStore.fontCaption
                    readonly property int valueSize: themeStore.fontCaption

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
                            var f = gpsStore.fix
                            if (!f || f === "none") return "—"
                            return f.toUpperCase()
                        }
                    }

                    InfoLabel { text: "Satellites" }
                    InfoValue {
                        text: gpsStore.satellitesUsed + " / " + gpsStore.satellitesVisible
                    }

                    InfoLabel { text: "SNR" }
                    InfoValue {
                        text: gpsStore.snr > 0 ? gpsStore.snr.toFixed(1) + " dB" : "—"
                    }

                    InfoLabel { text: "Accuracy" }
                    InfoValue {
                        text: gpsStore.eph > 0 ? "±" + gpsStore.eph.toFixed(1) + " m" : "—"
                    }

                    InfoLabel { text: "HDOP / PDOP" }
                    InfoValue {
                        text: (gpsStore.hdop > 0 ? gpsStore.hdop.toFixed(1) : "—")
                              + " / "
                              + (gpsStore.pdop > 0 ? gpsStore.pdop.toFixed(1) : "—")
                    }

                    InfoLabel { text: "Mode" }
                    InfoValue {
                        text: gpsStore.mode || "—"
                    }

                    InfoLabel { text: "Last TTFF" }
                    InfoValue {
                        text: gpsStore.lastTtffSeconds > 0
                              ? gpsStore.lastTtffSeconds.toFixed(0) + " s"
                                + (gpsStore.lastTtffMode ? " (" + gpsStore.lastTtffMode + ")" : "")
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
                notificationTopInset: typeof notificationService !== "undefined" && notificationService
                                     ? notificationService.occupiedHeight : 0
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
                y: (((typeof notificationService !== "undefined" && notificationService)
                     ? notificationService.occupiedHeight : 0) + parent.height) / 2
                   + (typeof mapService !== "undefined" ? mapService.vehicleOffsetY : 0) - height / 2
                visible: typeof mapService !== "undefined" && mapService.isReady
                transform: Rotation {
                    origin.x: vehicleMarkerItem.width / 2
                    origin.y: vehicleMarkerItem.height / 2
                    axis { x: 1; y: 0; z: 0 }
                    angle: (typeof settingsStore !== "undefined" && settingsStore.mapViewMode === 1) ? 0 : 55
                }
            }

            // Blinker icons (icon mode) — sit just below the turn-by-turn
            // banner when navigating, otherwise hug the top of the map.
            // Without this, the 56 px circles stack on top of the maneuver
            // icon (left) and time-info pill (right) inside the banner.
            BlinkerRow {
                anchors.top: (typeof notificationService === "undefined" || !notificationService)
                             && tbtWidget.visible ? tbtWidget.bottom
                             : ((typeof notificationService !== "undefined" && notificationService)
                                ? attentionDock.bottom : parent.top)
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.topMargin: 4
                z: 5
            }

            // Coverage is presented by the shared attention dock.

            // No-map message (shown when not navigating and no map position)
            Text {
                anchors.centerIn: parent
                visible: !mapScreen.hasNav && (typeof mapService === "undefined" || !mapService.isReady)
                text: typeof navigationService !== "undefined"
                      ? (typeof translations !== "undefined" ? translations.navSetDestination
                         : "Set a destination to start navigation")
                      : (typeof translations !== "undefined" ? translations.navUnavailable
                         : "Navigation unavailable")
                color: typeof themeStore !== "undefined" && themeStore.isDark
                       ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(0, 0, 0, 0.4)
                font.pixelSize: themeStore.fontBody
                horizontalAlignment: Text.AlignHCenter
            }

            // Turn-by-turn widget (top, full width)
            TurnByTurnWidget {
                id: tbtWidget
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                visible: (typeof notificationService === "undefined" || !notificationService)
                         && typeof navigationService !== "undefined"
                         && navigationService.isNavigating && navigationService.hasCurrentManeuver

                // North-up 2D centres the marker in the space the banner leaves,
                // so MapService needs to know whether the banner is up. Only the
                // visibility travels: the reserved height is a constant there, so
                // instruction text rewrapping cannot move the camera.
                Binding {
                    target: typeof mapService !== "undefined" ? mapService : null
                    property: "tbtVisible"
                    value: tbtWidget.visible
                }

                // Screens are Loader-swapped and DestinationScreen shares this
                // MapService with a centred crosshair, so hand the flag back
                // rather than leaving a stale true behind.
                Component.onDestruction: {
                    if (typeof mapService !== "undefined")
                        mapService.tbtVisible = false
                }
            }

            // Navigation status overlay (calculating, rerouting, arrived, error)
            NavigationStatusOverlay {
                visible: typeof notificationService === "undefined" || !notificationService
            }

            // Speed limit + road name (bottom center).
            RoadInfoRow {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                fontSize: 14
                onMapScreen: true
            }

            // North indicator (bottom-right, fixed position)
            NorthIndicator {
                anchors.right: parent.right
                anchors.bottom: scaleBar.top
                anchors.rightMargin: 8
                anchors.bottomMargin: 4
                visible: typeof mapService !== "undefined" && mapService.isReady
            }

            // Scale bar (bottom-right)
            ScaleBar {
                id: scaleBar
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                visible: typeof mapService !== "undefined" && mapService.isReady
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

    UnifiedAttentionDock {
        id: attentionDock
        anchors.top: parent.top
        anchors.topMargin: 40
        anchors.left: parent.left
        anchors.right: parent.right
        z: 20
    }

    Binding {
        target: typeof notificationService !== "undefined" ? notificationService : null
        property: "surface"
        value: "map"
    }
}
