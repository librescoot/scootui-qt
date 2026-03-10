import QtQuick

Item {
    id: versionOverlay
    anchors.fill: parent

    // Toggle::On = 0 in C++ enum
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
        radius: 8

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
                text: "MDB: N/A"
                font.pixelSize: 12
                color: parent.parent.textColor
            }

            Text {
                text: "DBC: N/A"
                font.pixelSize: 12
                color: parent.parent.textColor
            }

            Text {
                text: "nRF: N/A"
                font.pixelSize: 12
                color: parent.parent.textColor
            }

            Text {
                text: "ECU: N/A"
                font.pixelSize: 12
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
                      : "N/A")
                font.pixelSize: 12
                color: parent.parent.textColor
            }

            Text {
                text: "CBB: " + (typeof cbBatteryStore !== "undefined"
                      ? cbBatteryStore.charge + "%"
                      : "N/A")
                font.pixelSize: 12
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
                font.pixelSize: 11
                color: parent.parent.textColor
            }
        }
    }
}
