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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top status bar
        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        // Map area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

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

            // Road name (bottom center)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: roadNameText.width + 16
                height: roadNameText.height + 8
                radius: 4
                color: typeof themeStore !== "undefined" && themeStore.isDark
                       ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.8)
                visible: roadNameText.text.length > 0

                Text {
                    id: roadNameText
                    anchors.centerIn: parent
                    text: typeof speedLimitStore !== "undefined" ? speedLimitStore.roadName : ""
                    font.pixelSize: 14
                    color: typeof themeStore !== "undefined" && themeStore.isDark
                           ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                }
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
                            visible: typeof vehicleStore !== "undefined" && vehicleStore.isUnableToDrive === 0  // Toggle::On = 0
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
