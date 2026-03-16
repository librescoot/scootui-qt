import QtQuick
import QtQuick.Layouts

Rectangle {
    id: debugScreen
    color: "black"

    // Refresh trigger - incremented by timer to force property re-evaluation
    property int refreshTick: 0

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: debugScreen.refreshTick++
    }

    // Helper to safely read store properties
    function safeVal(storeAvailable, value, fallback) {
        return storeAvailable ? String(value) : (fallback || "N/A");
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#1565C0"

            Text {
                anchors.centerIn: parent
                text: "DEBUG MODE"
                color: "white"
                font.pixelSize: 11
                font.bold: true
            }
        }

        // Scrollable content
        Flickable {
            id: flickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: contentCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: flickable.width
                spacing: 0

                // Force re-evaluation on tick
                property int _tick: debugScreen.refreshTick

                // ---- Vehicle section ----
                DebugSection {
                    sectionTitle: "VEHICLE"
                    entries: [
                        { label: "State", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.stateRaw : "") }
                    ]
                }

                // ---- Engine section ----
                DebugSection {
                    sectionTitle: "ENGINE"
                    entries: [
                        { label: "Speed", value: debugScreen.safeVal(typeof engineStore !== "undefined", typeof engineStore !== "undefined" ? engineStore.speed.toFixed(1) + " km/h" : "") },
                        { label: "RPM", value: debugScreen.safeVal(typeof engineStore !== "undefined", typeof engineStore !== "undefined" ? engineStore.rpm.toFixed(0) + " RPM" : "") },
                        { label: "Odometer", value: debugScreen.safeVal(typeof engineStore !== "undefined", typeof engineStore !== "undefined" ? (engineStore.odometer / 1000).toFixed(1) + " km" : "") }
                    ]
                }

                // ---- Switches section ----
                DebugSection {
                    sectionTitle: "SWITCHES"
                    entries: [
                        { label: "Kickstand", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.kickstand : "") },
                        { label: "Seatbox Lock", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.seatboxLock : "") },
                        { label: "Brake L", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.brakeLeft : "") },
                        { label: "Brake R", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.brakeRight : "") },
                        { label: "Blinker", value: debugScreen.safeVal(typeof vehicleStore !== "undefined", typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : "") }
                    ]
                }

                // ---- GPS section ----
                DebugSection {
                    sectionTitle: "GPS"
                    entries: [
                        { label: "Latitude", value: debugScreen.safeVal(typeof gpsStore !== "undefined", typeof gpsStore !== "undefined" ? gpsStore.latitude.toFixed(6) : "") },
                        { label: "Longitude", value: debugScreen.safeVal(typeof gpsStore !== "undefined", typeof gpsStore !== "undefined" ? gpsStore.longitude.toFixed(6) : "") },
                        { label: "Altitude", value: debugScreen.safeVal(typeof gpsStore !== "undefined", typeof gpsStore !== "undefined" ? gpsStore.altitude.toFixed(1) + " m" : "") }
                    ]
                }

                // ---- Battery 0 section ----
                DebugSection {
                    sectionTitle: "BATTERY 0"
                    entries: [
                        { label: "Present", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.present : "") },
                        { label: "State", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.batteryState : "") },
                        { label: "Charge", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Battery 1 section ----
                DebugSection {
                    sectionTitle: "BATTERY 1"
                    entries: [
                        { label: "Present", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.present : "") },
                        { label: "State", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.batteryState : "") },
                        { label: "Charge", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Internet section ----
                DebugSection {
                    sectionTitle: "INTERNET"
                    entries: [
                        { label: "Modem", value: debugScreen.safeVal(typeof internetStore !== "undefined", typeof internetStore !== "undefined" ? internetStore.modemState : "") },
                        { label: "Status", value: debugScreen.safeVal(typeof internetStore !== "undefined", typeof internetStore !== "undefined" ? internetStore.status : "") },
                        { label: "Cloud", value: debugScreen.safeVal(typeof internetStore !== "undefined", typeof internetStore !== "undefined" ? internetStore.unuCloud : "") },
                        { label: "IP", value: debugScreen.safeVal(typeof internetStore !== "undefined", typeof internetStore !== "undefined" ? internetStore.ipAddress : "") }
                    ]
                }

                // ---- OTA section ----
                DebugSection {
                    sectionTitle: "OTA"
                    entries: [
                        { label: "DBC Status", value: debugScreen.safeVal(typeof otaStore !== "undefined", typeof otaStore !== "undefined" ? otaStore.dbcStatus : "") },
                        { label: "DBC Download", value: debugScreen.safeVal(typeof otaStore !== "undefined", typeof otaStore !== "undefined" ? otaStore.dbcDownloadProgress + "%" : "") }
                    ]
                }

                // Bottom padding
                Item { Layout.preferredHeight: 16 }
            }
        }
    }

    // Inline component for debug sections
    component DebugSection: ColumnLayout {
        id: debugSection
        property string sectionTitle: ""
        property var entries: []

        Layout.fillWidth: true
        spacing: 0

        // Section header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            color: "#0D47A1"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: debugSection.sectionTitle
                color: "#90CAF9"
                font.pixelSize: 10
                font.bold: true
            }
        }

        // Rows
        Repeater {
            model: debugSection.entries

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                color: index % 2 === 0 ? "#0A0A0A" : "black"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 4

                    Text {
                        Layout.preferredWidth: parent.width * 0.4
                        text: modelData.label
                        color: "#9E9E9E"
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.value
                        color: "white"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
