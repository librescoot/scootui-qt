import QtQuick
import QtQuick.Layouts

Rectangle {
    id: aboutScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color accentColor: isDark ? "#40C8F0" : "#0090B8"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

    // Warning box colors
    readonly property color warningBg: isDark ? "#1A1200" : "#FFF8E1"
    readonly property color warningBorder: isDark ? "#5C4400" : "#FFB300"
    readonly property color warningText: isDark ? "#FFB300" : "#5C4400"

    // FOSS component data
    readonly property var fossComponents: [
        { name: "Qt", license: "LGPL-3.0" },
        { name: "hiredis", license: "BSD-3-Clause" },
        { name: "redis-plus-plus", license: "Apache-2.0" }
    ]

    // Scroll handling via brake inputs
    Connections {
        target: typeof vehicleStore !== "undefined" ? vehicleStore : null
        function onBrakeLeftChanged() {
            if (typeof vehicleStore !== "undefined" && vehicleStore.brakeLeft === 1) {
                flickable.contentY = Math.min(
                    flickable.contentY + 60,
                    flickable.contentHeight - flickable.height
                );
            }
        }
        function onBrakeRightChanged() {
            if (typeof vehicleStore !== "undefined" && vehicleStore.brakeRight === 1) {
                if (typeof screenStore !== "undefined") {
                    screenStore.setScreen(0);
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Header (non-scrolling) ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 24
            spacing: 4

            Text {
                text: "LibreScoot"
                color: aboutScreen.textPrimary
                font.pixelSize: 28
                font.bold: true
                font.letterSpacing: 0.5
            }

            Text {
                text: "ScootUI"
                color: aboutScreen.textSecondary
                font.pixelSize: 14
                font.letterSpacing: 1.0
            }

            Item { Layout.preferredHeight: 4 }

            Text {
                text: "https://librescoot.org"
                color: aboutScreen.accentColor
                font.pixelSize: 13
            }

            Text {
                text: "CC BY-NC-SA 4.0 \u00A9 2025 LibreScoot contributors"
                color: aboutScreen.textSecondary
                font.pixelSize: 12
            }

            Item { Layout.preferredHeight: 8 }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: aboutScreen.dividerColor
        }

        // ---- Scrollable content ----
        Flickable {
            id: flickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: scrollContent.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: scrollContent
                width: flickable.width
                spacing: 0

                Item { Layout.preferredHeight: 16 }

                // Non-Commercial Warning Box
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.preferredHeight: warningCol.height + 20
                    color: aboutScreen.warningBg
                    border.color: aboutScreen.warningBorder
                    border.width: 1
                    radius: 6

                    ColumnLayout {
                        id: warningCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            text: "Non-Commercial License"
                            color: aboutScreen.warningText
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "This software is licensed for non-commercial use only."
                            color: aboutScreen.warningText
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { Layout.preferredHeight: 16 }

                // Divider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: aboutScreen.dividerColor
                }

                Item { Layout.preferredHeight: 12 }

                // FOSS Components header
                Text {
                    Layout.leftMargin: 24
                    text: "Open Source Components"
                    color: aboutScreen.textSecondary
                    font.pixelSize: 12
                    font.bold: true
                }

                Item { Layout.preferredHeight: 8 }

                // FOSS component list
                Repeater {
                    model: aboutScreen.fossComponents

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.topMargin: 6
                            Layout.bottomMargin: 6

                            Text {
                                text: modelData.name
                                color: aboutScreen.textPrimary
                                font.pixelSize: 13
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: modelData.license
                                color: aboutScreen.textSecondary
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: aboutScreen.dividerColor
                        }
                    }
                }

                // Bottom padding for scroll
                Item { Layout.preferredHeight: 60 }
            }
        }

        // ---- Footer (non-scrolling) ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: aboutScreen.dividerColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: "Scroll"
                color: aboutScreen.textSecondary
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Back"
                color: aboutScreen.textSecondary
                font.pixelSize: 12
            }
        }
    }
}
