import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/navigation"
import "../widgets/indicators"
import "../widgets/map"

Rectangle {
    id: mapScreen
    color: "black"

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

            // Vehicle marker at fixed screen position
            VehicleMarker {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height / 2 + (typeof mapService !== "undefined" ? mapService.vehicleOffsetY : 0) - 18
                visible: typeof mapService !== "undefined" && mapService.isReady
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

            // Speed indicator (bottom-left)
            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 8
                width: speedCol.width + 16
                height: speedCol.height + 12
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.7)

                ColumnLayout {
                    id: speedCol
                    anchors.centerIn: parent
                    spacing: 0

                    Text {
                        text: typeof engineStore !== "undefined"
                              ? Math.floor(engineStore.speed) : "0"
                        font.pixelSize: 32
                        font.bold: true
                        color: "white"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "km/h"
                        font.pixelSize: 10
                        color: Qt.rgba(1, 1, 1, 0.6)
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Speed limit indicator (bottom-left, above speed)
            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.bottomMargin: 72
                width: 40
                height: 40
                radius: 20
                color: "white"
                border.color: "red"
                border.width: 3
                visible: typeof speedLimitStore !== "undefined" &&
                         speedLimitStore.speedLimit.length > 0 &&
                         speedLimitStore.speedLimit !== "none"

                Text {
                    anchors.centerIn: parent
                    text: typeof speedLimitStore !== "undefined" ? speedLimitStore.speedLimit : ""
                    font.pixelSize: 14
                    font.bold: true
                    color: "black"
                }
            }

            // Road name (bottom center)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: roadNameText.width + 16
                height: roadNameText.height + 8
                radius: 4
                color: Qt.rgba(0, 0, 0, 0.6)
                visible: roadNameText.text.length > 0

                Text {
                    id: roadNameText
                    anchors.centerIn: parent
                    text: typeof speedLimitStore !== "undefined" ? speedLimitStore.roadName : ""
                    font.pixelSize: 14
                    color: "white"
                }
            }

            // North indicator + Scale bar (bottom-right)
            Column {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 48
                spacing: 4
                visible: typeof mapService !== "undefined" && mapService.isReady

                NorthIndicator {}
                ScaleBar {}
            }

            // Warning indicators (bottom right)
            StatusIndicators {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
            }
        }
    }
}
