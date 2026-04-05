import QtQuick

Item {
    id: versionOverlay
    anchors.fill: parent

    // Toggle::On = 0, Off = 1 in C++ enum (checking Enums.h)
    // Wait, let me check Enums.h again:
    // enum class Toggle { On, Off }; -> On=0, Off=1
    // So brakeLeft === 0 means On.

    property bool bothBrakes: typeof vehicleStore !== "undefined"
                              ? (vehicleStore.brakeLeft === 0 && vehicleStore.brakeRight === 0)
                              : false
    // Use property binding (not Q_INVOKABLE method) for reactivity
    // ScooterState::Parked = 4
    property bool canShow: typeof vehicleStore !== "undefined" && typeof menuStore !== "undefined"
                           ? (vehicleStore.state === 4 && !menuStore.isOpen)
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
        if (showOverlay && typeof systemInfoService !== "undefined")
            systemInfoService.loadVersions()
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
        radius: themeStore.radiusCard

        color: typeof themeStore !== "undefined" && themeStore.isDark
               ? "#B3000000"   // black 0.7 opacity
               : "#B3FFFFFF"   // white 0.7 opacity
        border.width: 1.5
        border.color: typeof themeStore !== "undefined" && themeStore.isDark
                      ? "#FFFFFF"
                      : "#000000"

        property color textColor: typeof themeStore !== "undefined" && themeStore.isDark
                                  ? "#FFFFFF" : "#000000"

        Column {
            id: infoColumn
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: "MDB: " + (typeof systemInfoService !== "undefined"
                      ? systemInfoService.mdbVersion : "unknown")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "DBC: " + (typeof systemInfoService !== "undefined"
                      ? systemInfoService.dbcVersion : "unknown")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "nRF: " + (typeof systemInfoService !== "undefined"
                      ? systemInfoService.nrfVersion : "unknown")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "ECU: " + (typeof systemInfoService !== "undefined"
                      ? systemInfoService.ecuVersion : "unknown")
                font.pixelSize: themeStore.fontBody
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
                text: "AUX: " + (typeof auxBatteryStore !== "undefined"
                      ? auxBatteryStore.voltage + "mV " + auxBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }

            Text {
                text: "CBB: " + (typeof cbBatteryStore !== "undefined"
                      ? cbBatteryStore.charge + "%"
                      : "unknown")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }

            // Serial number
            Rectangle {
                visible: typeof serialNumberService !== "undefined" && serialNumberService.available
                width: parent.width
                height: 1
                color: parent.parent.textColor
                opacity: 0.3
            }

            Text {
                visible: typeof serialNumberService !== "undefined" && serialNumberService.available
                text: "S/N: " + (typeof serialNumberService !== "undefined"
                      ? serialNumberService.serialNumber : "")
                font.pixelSize: themeStore.fontBody
                color: parent.parent.textColor
            }
        }
    }
}
