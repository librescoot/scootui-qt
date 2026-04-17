import QtQuick
import ScootUI 1.0

Item {
    id: versionOverlay
    anchors.fill: parent

    property bool bothBrakes: true
                              ? (VehicleStore.brakeLeft === 0 && VehicleStore.brakeRight === 0)
                              : false
    property bool canShow: true
                           ? (VehicleStore.state === 4 && !MenuStore.isOpen)
                           : false
    property bool showOverlay: false

    visible: showOverlay

    Timer {
        id: holdTimer
        interval: 3000
        running: versionOverlay.bothBrakes && versionOverlay.canShow && !versionOverlay.showOverlay
        onTriggered: versionOverlay.showOverlay = true
    }

    onShowOverlayChanged: {
        if (showOverlay && true)
            SystemInfoService.loadVersions()
    }

    Timer {
        id: lingerTimer
        interval: 4000
        onTriggered: versionOverlay.showOverlay = false
    }

    onBothBrakesChanged: {
        if (!bothBrakes) {
            lingerTimer.start()
        } else {
            lingerTimer.stop()
        }
    }

    // Positioned bottom-right
    Rectangle {
        id: versionCard
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 80
        anchors.rightMargin: 20
        width: infoColumn.width + 24
        height: infoColumn.height + 24
        radius: ThemeStore.radiusCard

        color: ThemeStore.isDark
               ? "#B3000000"   // black 0.7 opacity
               : "#B3FFFFFF"   // white 0.7 opacity
        border.width: 1.5
        border.color: ThemeStore.isDark
                      ? "#FFFFFF"
                      : "#000000"

        property color textColor: ThemeStore.isDark
                                  ? "#FFFFFF" : "#000000"

        Column {
            id: infoColumn
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: "MDB: " + (SystemInfoService.mdbVersion)
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            Text {
                text: "DBC: " + (SystemInfoService.dbcVersion)
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            Text {
                text: "nRF: " + (SystemInfoService.nrfVersion)
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            Text {
                text: "ECU: " + (SystemInfoService.ecuVersion)
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: versionCard.textColor
                opacity: 0.3
            }

            Text {
                text: "AUX: " + (true
                      ? AuxBatteryStore.voltage + "mV " + AuxBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            Text {
                text: "CBB: " + (true
                      ? CbBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }

            // Serial number
            Rectangle {
                visible: SerialNumberService.available
                width: parent.width
                height: 1
                color: versionCard.textColor
                opacity: 0.3
            }

            Text {
                visible: SerialNumberService.available
                text: "S/N: " + (SerialNumberService.serialNumber)
                font.pixelSize: ThemeStore.fontBody
                color: versionCard.textColor
            }
        }
    }
}
