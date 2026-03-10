import QtQuick
import QtQuick.Layouts

Item {
    id: debugOverlay
    anchors.fill: parent
    z: 50
    visible: typeof dashboardStore !== "undefined" && dashboardStore.debugMode === "overlay"

    // Semi-transparent background panels
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 4
        width: debugCol.width + 12
        height: debugCol.height + 12
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.75)

        ColumnLayout {
            id: debugCol
            anchors.centerIn: parent
            spacing: 2

            // Vehicle state
            Text {
                text: "VEH: " + (typeof vehicleStore !== "undefined" ? vehicleStore.stateRaw : "?")
                color: "#00FF00"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "BRK: L=" + (typeof vehicleStore !== "undefined" ? vehicleStore.brakeLeft : "?")
                      + " R=" + (typeof vehicleStore !== "undefined" ? vehicleStore.brakeRight : "?")
                color: "#00FF00"
                font.pixelSize: 10
                font.family: "monospace"
            }

            // GPS
            Text {
                text: "GPS: " + (typeof gpsStore !== "undefined"
                    ? gpsStore.latitude.toFixed(5) + "," + gpsStore.longitude.toFixed(5)
                    : "N/A")
                color: "#00FFFF"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "SPD: " + (typeof gpsStore !== "undefined" ? gpsStore.speed.toFixed(1) : "?")
                      + " ST: " + (typeof gpsStore !== "undefined" ? gpsStore.gpsState : "?")
                color: "#00FFFF"
                font.pixelSize: 10
                font.family: "monospace"
            }

            // Engine
            Text {
                text: "THR: " + (typeof engineStore !== "undefined" ? engineStore.throttle : "?")
                      + " RPM: " + (typeof engineStore !== "undefined" ? Math.floor(engineStore.rpm) : "?")
                color: "#FFFF00"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "PWR: " + (typeof engineStore !== "undefined"
                    ? (engineStore.motorVoltage * engineStore.motorCurrent).toFixed(0) + "W"
                    : "?")
                color: "#FFFF00"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "TMP: " + (typeof engineStore !== "undefined"
                    ? engineStore.temperature.toFixed(0) + "C" : "?")
                color: "#FFFF00"
                font.pixelSize: 10
                font.family: "monospace"
            }

            // Internet
            Text {
                text: "NET: " + (typeof internetStore !== "undefined" ? internetStore.modemState : "?")
                      + " " + (typeof internetStore !== "undefined" ? internetStore.accessTech : "")
                color: "#FF88FF"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "SIG: " + (typeof internetStore !== "undefined" ? internetStore.signalQuality : "?")
                      + " CLD: " + (typeof internetStore !== "undefined" ? internetStore.unuCloud : "?")
                color: "#FF88FF"
                font.pixelSize: 10
                font.family: "monospace"
            }
        }
    }

    // Battery info panel (right side)
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        width: battCol.width + 12
        height: battCol.height + 12
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.75)

        ColumnLayout {
            id: battCol
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: "B0: " + (typeof battery0Store !== "undefined"
                    ? battery0Store.charge + "% " + battery0Store.voltage + "mV "
                      + battery0Store.current + "mA"
                    : "N/A")
                color: "#88FF88"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "B0c: " + (typeof battery0Store !== "undefined"
                    ? battery0Store.cycleCount + " FW:" + battery0Store.firmwareVersion
                    : "")
                color: "#88FF88"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "B1: " + (typeof battery1Store !== "undefined" && battery1Store.present
                    ? battery1Store.charge + "% " + battery1Store.voltage + "mV "
                      + battery1Store.current + "mA"
                    : "N/A")
                color: "#88FF88"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "AUX: " + (typeof auxBatteryStore !== "undefined"
                    ? auxBatteryStore.voltage + "mV " + auxBatteryStore.charge + "%"
                    : "N/A")
                color: "#FFAA44"
                font.pixelSize: 10
                font.family: "monospace"
            }

            Text {
                text: "CBB: " + (typeof cbBatteryStore !== "undefined"
                    ? cbBatteryStore.charge + "% " + cbBatteryStore.temperature + "C"
                    : "N/A")
                color: "#FFAA44"
                font.pixelSize: 10
                font.family: "monospace"
            }
        }
    }
}
