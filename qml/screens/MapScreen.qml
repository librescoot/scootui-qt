import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/navigation"
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
    property bool mapReady: typeof mapService !== "undefined" && mapService.isReady

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
                    text: "\ue2dd" // gps_not_fixed
                    font.family: "Material Icons"
                    font.pixelSize: 48
                    color: typeof themeStore !== "undefined" && themeStore.isDark
                           ? "#99FFFFFF" : "#8A000000"  // white60 / black54
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.mapWaitingForGps : "Waiting for GPS fix"
                    font.pixelSize: 18
                    color: typeof themeStore !== "undefined" && themeStore.isDark
                           ? "#FFFFFF" : "#000000"
                    horizontalAlignment: Text.AlignHCenter
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

            // Vehicle marker at fixed screen position, tilted to match 3D map
            VehicleMarker {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height / 2 + (typeof mapService !== "undefined" ? mapService.vehicleOffsetY : 0) - 18
                visible: typeof mapService !== "undefined" && mapService.isReady
                transform: Rotation {
                    origin.x: 18.5
                    origin.y: 18.5
                    axis { x: 1; y: 0; z: 0 }
                    angle: 55
                }
            }

            // Out of coverage overlay (Flutter: _buildOutOfCoverageOverlay)
            // Floating pill at top when GPS is outside mbtiles bounds
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 8
                width: outOfCoverageRow.width + 24  // padding h:12
                height: outOfCoverageRow.height + 16  // padding v:8
                radius: 8
                color: typeof themeStore !== "undefined" && themeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
                border.width: 1.5
                border.color: Qt.rgba(1, 0.647, 0, 0.6)  // orange with 60% opacity
                visible: typeof mapService !== "undefined" && mapService.isOutOfCoverage
                z: 10

                Row {
                    id: outOfCoverageRow
                    anchors.centerIn: parent
                    spacing: 8

                    // map_outlined icon (Flutter: Icons.map_outlined, color: orange, size: 16)
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\uf1ae" // map_outlined
                        font.family: "Material Icons"
                        font.pixelSize: 16
                        color: "#FF9800"  // Colors.orange
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: typeof translations !== "undefined"
                              ? translations.mapOutOfCoverage : "No map data for current location"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: "#FF9800"  // Colors.orange
                    }
                }
            }

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
                font.pixelSize: 16
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

            // Warning telltales (bottom left) - engine warning, hazards, parking brake
            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 8
                spacing: 8
                visible: showWarnings

                property bool showWarnings: {
                    if (typeof vehicleStore === "undefined") return false
                    return vehicleStore.isUnableToDrive === 0  // Toggle::On = 0
                           || (typeof connectionStore !== "undefined" && connectionStore.usingBackupConnection)
                           || vehicleStore.blinkerState === 3
                           || vehicleStore.state === 4  // Parked
                }

                Rectangle {
                    width: warningRow.width + 16
                    height: warningRow.height + 16
                    radius: 8
                    color: typeof themeStore !== "undefined" && themeStore.isDark
                           ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9)
                    border.width: 1
                    border.color: typeof themeStore !== "undefined" && themeStore.isDark
                                  ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.12)

                    Row {
                        id: warningRow
                        anchors.centerIn: parent
                        spacing: 8

                        // Engine warning
                        IndicatorLight {
                            visible: (typeof vehicleStore !== "undefined" && vehicleStore.isUnableToDrive === 0)
                                     || (typeof connectionStore !== "undefined" && connectionStore.usingBackupConnection)
                            source: "qrc:/ScootUI/assets/icons/librescoot-engine-warning.svg"
                            active: true
                            blinking: false
                            tintColor: "#FFC107"
                            width: 32; height: 32
                        }

                        // Hazards
                        IndicatorLight {
                            visible: typeof vehicleStore !== "undefined" && vehicleStore.blinkerState === 3
                            source: "qrc:/ScootUI/assets/icons/librescoot-hazards.svg"
                            active: true
                            blinking: true
                            tintColor: "#F44336"
                            width: 32; height: 32
                        }

                        // Parking brake
                        IndicatorLight {
                            visible: typeof vehicleStore !== "undefined" && vehicleStore.state === 4
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
            Layout.fillWidth: true

            SpeedCenterWidget {
                // Placed as center widget in the bottom bar
                // Note: UnifiedBottomStatusBar currently has empty center;
                // speed is shown here matching Flutter's MapScreen layout
            }
        }
    }
}
