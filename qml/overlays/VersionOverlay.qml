import QtQuick
import ScootUI

Item {
    id: versionOverlay
    anchors.fill: parent

    // Toggle::On = 0, Off = 1 in C++ enum (checking Enums.h)
    // Wait, let me check Enums.h again:
    // enum class Toggle { On, Off }; -> On=0, Off=1
    // So brakeLeft === 0 means On.

    property bool bothBrakes: VehicleStore.brakeLeft === 0 && VehicleStore.brakeRight === 0
    // Use property binding (not Q_INVOKABLE method) for reactivity
    // ScooterState::Parked = 4
    property bool canShow: VehicleStore.state === 4 && !MenuStore.isOpen
    property bool showOverlay: false

    visible: showOverlay

    Timer {
        id: holdTimer
        interval: 3000
        running: versionOverlay.bothBrakes && versionOverlay.canShow && !versionOverlay.showOverlay
        onTriggered: versionOverlay.showOverlay = true
    }

    onShowOverlayChanged: {
        if (showOverlay)
            SystemInfoService.loadVersions()
    }

    onBothBrakesChanged: {
        if (!bothBrakes) {
            showOverlay = false
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
                text: "MDB: " + SystemInfoService.mdbVersion
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "DBC: " + SystemInfoService.dbcVersion
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "nRF: " + SystemInfoService.nrfVersion
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "ECU: " + SystemInfoService.ecuVersion
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
                text: "AUX: " + AuxBatteryStore.voltage + "mV " + AuxBatteryStore.charge + "%"
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "CBB: " + CbBatteryStore.charge + "%"
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }

            // Serial number
            Rectangle {
                visible: SerialNumberService.available
                width: parent.width
                height: 1
                color: parent.parent.textColor
                opacity: 0.3
            }

            Text {
                visible: SerialNumberService.available
                text: "S/N: " + SerialNumberService.serialNumber
                font.pixelSize: ThemeStore.fontBody
                color: parent.parent.textColor
            }
        }
    }
}
