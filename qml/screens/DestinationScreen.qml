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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        // Map area with destination markers
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            MapViewWidget {
                anchors.fill: parent
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
                radius: 6
                color: Qt.rgba(0, 0, 0, 0.7)

                Text {
                    id: coordText
                    anchors.centerIn: parent
                    text: typeof mapService !== "undefined"
                          ? mapService.mapLatitude.toFixed(5) + ", " + mapService.mapLongitude.toFixed(5)
                          : "N/A"
                    color: "white"
                    font.pixelSize: 13
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
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            Text {
                text: typeof translations !== "undefined" ? translations.controlBack : "Back"
                color: destinationScreen.textSecondary
                font.pixelSize: 12
            }
        }
    }
}
