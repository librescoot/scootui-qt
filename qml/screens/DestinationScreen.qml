import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/map"

Rectangle {
    id: destinationScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"

    // Right brake returns to map (centralized via InputHandler)
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onRightTap() {
            if (typeof screenStore !== "undefined") {
                screenStore.setScreen(1) // Back to map
            }
        }
    }

    readonly property bool mapsAvailable: typeof navAvailabilityService !== "undefined"
                                          ? navAvailabilityService.localDisplayMapsAvailable : false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        // Offline indicator (Flutter: mapState is! MapOffline → location_off + text)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !destinationScreen.mapsAvailable

            Column {
                anchors.centerIn: parent
                width: parent.width - 40  // Flutter: padding 20 on each side
                spacing: 16

                // location_off icon (Flutter: Icons.location_off, size: 48, color: grey)
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\ue3aa" // location_off
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontXL
                    color: "#9E9E9E"  // Colors.grey
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.destinationOfflineOnly
                          : "The destination selector only works with offline maps"
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                    color: destinationScreen.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.destinationInstallMapData
                          : "Please install the map data to use this feature"
                    font.pixelSize: themeStore.fontBody
                    color: "#9E9E9E"  // Colors.grey
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }

        // Map area with destination markers
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: destinationScreen.mapsAvailable

            MapViewWidget {
                anchors.fill: parent
            }

            // Saved location markers (Flutter: CameraFilteredMarkerLayer with location_pin at zoom >= 17.5)
            Repeater {
                model: typeof savedLocationsStore !== "undefined" ? savedLocationsStore.locations : []
                delegate: Item {
                    // Only show markers when zoom >= 17.5 (matches Flutter's CameraFilteredMarkerLayer)
                    visible: typeof mapService !== "undefined" && mapService.mapZoom >= 17.5
                    // Marker positioning would require lat/lng to screen coordinate conversion
                    // which depends on the MapLibre GL integration; placeholder for now
                }
            }

            // Crosshair at center
            Rectangle {
                anchors.centerIn: parent
                width: 2
                height: 24
                color: destinationScreen.isDark ? "white" : "black"
                opacity: 0.6
            }
            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 2
                color: destinationScreen.isDark ? "white" : "black"
                opacity: 0.6
            }

            // Coordinate overlay at bottom
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 12
                width: coordText.width + 24
                height: coordText.height + 12
                radius: themeStore.radiusCard
                color: Qt.rgba(0, 0, 0, 0.7)

                Text {
                    id: coordText
                    anchors.centerIn: parent
                    text: typeof mapService !== "undefined"
                          ? mapService.mapLatitude.toFixed(5) + ", " + mapService.mapLongitude.toFixed(5)
                          : "N/A"
                    color: "white"
                    font.pixelSize: themeStore.fontFeature
                }
            }
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: typeof translations !== "undefined" ? translations.navConfirmDest : "Confirm"
                color: destinationScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }

            Item { Layout.fillWidth: true }

            Text {
                text: typeof translations !== "undefined" ? translations.controlBack : "Back"
                color: destinationScreen.textSecondary
                font.pixelSize: themeStore.fontBody
            }
        }
    }
}
