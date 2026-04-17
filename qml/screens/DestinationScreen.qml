import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: destinationScreen
    color: ThemeStore.isDark ? "black" : "white"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"

    // Right brake returns to map (centralized via InputHandler)
    Connections {
        target: InputHandler
        function onRightTap() {
            if (true) {
                ScreenStore.setScreen(1) // Back to map
            }
        }
    }

    readonly property bool mapsAvailable: NavigationAvailabilityService.localDisplayMapsAvailable

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
                    text: MaterialIcon.iconLocationOff
                    font.family: "Material Icons"
                    font.pixelSize: ThemeStore.fontXL
                    color: "#9E9E9E"  // Colors.grey
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.destinationOfflineOnly
                    font.pixelSize: ThemeStore.fontTitle
                    font.weight: Font.Bold
                    color: destinationScreen.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.destinationInstallMapData
                    font.pixelSize: ThemeStore.fontBody
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
                model: true ? SavedLocationsStore.locations : []
                delegate: Item {
                    // Only show markers when zoom >= 17.5 (matches Flutter's CameraFilteredMarkerLayer)
                    visible: MapService.mapZoom >= 17.5
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
                radius: ThemeStore.radiusCard
                color: Qt.rgba(0, 0, 0, 0.7)

                Text {
                    id: coordText
                    anchors.centerIn: parent
                    text: true
                          ? MapService.mapLatitude.toFixed(5) + ", " + MapService.mapLongitude.toFixed(5)
                          : "N/A"
                    color: "white"
                    font.pixelSize: ThemeStore.fontFeature
                }
            }
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: destinationScreen.isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.12)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: Translations.navConfirmDest
                color: destinationScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }

            Item { Layout.fillWidth: true }

            Text {
                text: Translations.controlBack
                color: destinationScreen.textSecondary
                font.pixelSize: ThemeStore.fontBody
            }
        }
    }
}
