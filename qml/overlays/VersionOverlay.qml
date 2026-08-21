import QtQuick
import ScootUI 1.0

Item {
    id: versionOverlay
    anchors.fill: parent

    property bool bothBrakes: typeof VehicleStore !== "undefined"
                              ? (VehicleStore.brakeLeft === 0 && VehicleStore.brakeRight === 0)
                              : false
    property bool canShow: typeof VehicleStore !== "undefined" && typeof MenuStore !== "undefined"
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
        if (showOverlay && typeof SystemInfoService !== "undefined")
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
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 80
        anchors.rightMargin: 20
        width: infoColumn.width + 24
        height: infoColumn.height + 24
        radius: ThemeStore.radiusCard

        color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
               ? "#B3000000"   // black 0.7 opacity
               : "#B3FFFFFF"   // white 0.7 opacity
        border.width: 1.5
        border.color: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                      ? "#FFFFFF"
                      : "#000000"

        property color textColor: typeof ThemeStore !== "undefined" && ThemeStore.isDark
                                  ? "#FFFFFF" : "#000000"

        Column {
            id: infoColumn
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: "MDB: " + (typeof SystemInfoService !== "undefined"
                      ? SystemInfoService.mdbVersion : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "DBC: " + (typeof SystemInfoService !== "undefined"
                      ? SystemInfoService.dbcVersion : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "nRF: " + (typeof SystemInfoService !== "undefined"
                      ? SystemInfoService.nrfVersion : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "ECU: " + (typeof SystemInfoService !== "undefined"
                      ? SystemInfoService.ecuVersion : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: parent.parent.textColor
                opacity: 0.3
            }

            Text {
                text: "AUX: " + (typeof AuxBatteryStore !== "undefined"
                      ? AuxBatteryStore.voltage + "mV " + AuxBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "CBB: " + (typeof CbBatteryStore !== "undefined"
                      ? CbBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            // Serial number
            Rectangle {
                visible: typeof SerialNumberService !== "undefined" && SerialNumberService.available
                width: parent.width
                height: 1
                color: parent.parent.textColor
                opacity: 0.3
            }

            Text {
                visible: typeof SerialNumberService !== "undefined" && SerialNumberService.available
                text: "S/N: " + (typeof SerialNumberService !== "undefined"
                      ? SerialNumberService.serialNumber : "")
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }
        }
    }
}
