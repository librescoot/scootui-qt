import QtQuick

Row {
    id: batteryDisplay
    spacing: 4
    height: 24

    // Battery 0
    readonly property int charge0: typeof battery0Store !== "undefined" ? battery0Store.charge : 0
    readonly property bool present0: typeof battery0Store !== "undefined" ? battery0Store.present : false

    // Battery 1 (dual battery mode)
    readonly property int charge1: typeof battery1Store !== "undefined" ? battery1Store.charge : 0
    readonly property bool present1: typeof battery1Store !== "undefined" ? battery1Store.present : false
    readonly property bool showDual: present1

    // Seatbox
    // SeatboxLock::Open = 0, SeatboxLock::Closed = 1
    readonly property bool seatboxOpen: typeof vehicleStore !== "undefined" ? vehicleStore.seatboxLock === 0 : false

    // --- Battery 0 icon with charge fill ---
    Item {
        width: 24
        height: 24

        Image {
            anchors.fill: parent
            source: {
                if (!present0) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-absent.svg"
                if (charge0 <= 10) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-empty.svg"
                return "qrc:/ScootUI/assets/icons/librescoot-main-battery-blank.svg"
            }
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }

        // Charge level rectangle overlay
        Rectangle {
            visible: present0 && charge0 > 10
            x: 3.84
            y: 6.84
            height: 13.86
            width: 16.37 * (charge0 / 100)
            color: {
                if (charge0 <= 10) return "#FF0000"
                if (charge0 <= 20) return "#FF7900"
                return themeStore.textColor
            }
            radius: 1
        }
    }

    // Battery 0 charge text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: present0 ? charge0 + "%" : ""
        font.pixelSize: 16
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: {
            if (charge0 <= 10) return "#FF0000"
            if (charge0 <= 20) return "#FF7900"
            return themeStore.textColor
        }
    }

    // --- Battery 1 (dual mode) ---
    Item {
        width: 24
        height: 24
        visible: batteryDisplay.showDual

        Image {
            anchors.fill: parent
            source: {
                if (!present1) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-absent.svg"
                if (charge1 <= 10) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-empty.svg"
                return "qrc:/ScootUI/assets/icons/librescoot-main-battery-blank.svg"
            }
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
        }

        Rectangle {
            visible: present1 && charge1 > 10
            x: 3.84
            y: 6.84
            height: 13.86
            width: 16.37 * (charge1 / 100)
            color: {
                if (charge1 <= 10) return "#FF0000"
                if (charge1 <= 20) return "#FF7900"
                return themeStore.textColor
            }
            radius: 1
        }
    }

    // Battery 1 charge text
    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: batteryDisplay.showDual
        text: present1 ? charge1 + "%" : ""
        font.pixelSize: 16
        font.weight: Font.DemiBold
        font.letterSpacing: -1.1
        color: {
            if (charge1 <= 10) return "#FF0000"
            if (charge1 <= 20) return "#FF7900"
            return themeStore.textColor
        }
    }

    // --- Seatbox open indicator (after batteries, like Flutter) ---
    Item {
        visible: batteryDisplay.seatboxOpen
        width: 24
        height: 24
        anchors.verticalCenter: parent.verticalCenter

        Image {
            anchors.fill: parent
            source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-open.svg"
            sourceSize: Qt.size(24, 24)
            fillMode: Image.PreserveAspectFit
            // White SVG visible in dark mode
            visible: typeof themeStore !== "undefined" && themeStore.isDark
        }

        // Light mode: dark circle background with white icon
        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "#333333"
            visible: typeof themeStore !== "undefined" && !themeStore.isDark

            Image {
                anchors.fill: parent
                anchors.margins: 2
                source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-open.svg"
                sourceSize: Qt.size(20, 20)
                fillMode: Image.PreserveAspectFit
            }
        }
    }
}
