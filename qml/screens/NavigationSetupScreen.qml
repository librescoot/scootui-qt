import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"

Rectangle {
    id: navSetupScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#0090B8"
    readonly property color checkColor: "#4CAF50"
    readonly property color crossColor: "#F44336"

    readonly property bool mapsOk: typeof navAvailabilityService !== "undefined"
                                    ? navAvailabilityService.localDisplayMapsAvailable : false
    readonly property bool routingOk: typeof navAvailabilityService !== "undefined"
                                       ? navAvailabilityService.routingAvailable : false

    // Right brake returns to cluster
    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onBrakeRightChanged() {
            if (typeof vehicleStore !== "undefined" && vehicleStore.brakeRight === 1) {
                if (typeof screenStore !== "undefined") {
                    screenStore.setScreen(0)
                }
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

        Item { Layout.fillHeight: true }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: typeof translations !== "undefined" ? translations.navUnavailable : "Navigation unavailable"
            color: navSetupScreen.textPrimary
            font.pixelSize: 20
            font.bold: true
        }

        Item { Layout.preferredHeight: 24 }

        // Status rows
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            // Local display maps
            RowLayout {
                spacing: 8
                Rectangle {
                    width: 24; height: 24; radius: 12
                    color: navSetupScreen.mapsOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                    Text {
                        anchors.centerIn: parent
                        text: navSetupScreen.mapsOk ? "\u2713" : "\u2717"
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
                Text {
                    text: "Local Display Maps"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: 14
                }
            }

            // Routing service
            RowLayout {
                spacing: 8
                Rectangle {
                    width: 24; height: 24; radius: 12
                    color: navSetupScreen.routingOk ? navSetupScreen.checkColor : navSetupScreen.crossColor
                    Text {
                        anchors.centerIn: parent
                        text: navSetupScreen.routingOk ? "\u2713" : "\u2717"
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
                Text {
                    text: "Routing Service"
                    color: navSetupScreen.textPrimary
                    font.pixelSize: 14
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        // Instructions
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            text: "Visit librescoot.org/docs/navigation\nfor setup instructions"
            color: navSetupScreen.textSecondary
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        // Footer control hint
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

            Item { Layout.fillWidth: true }

            Text {
                text: typeof translations !== "undefined" ? translations.controlBack : "Back"
                color: navSetupScreen.textSecondary
                font.pixelSize: 12
            }
        }
    }
}
