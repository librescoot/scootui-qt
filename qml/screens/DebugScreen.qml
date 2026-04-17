import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

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
                        { label: "State", value: debugScreen.safeVal(true, VehicleStore.stateRaw) }
                    ]
                }

                // ---- Engine section ----
                DebugSection {
                    sectionTitle: "ENGINE"
                    entries: [
                        { label: "Speed", value: debugScreen.safeVal(true, true ? EngineStore.speed.toFixed(1) + " km/h" : "") },
                        { label: "RPM", value: debugScreen.safeVal(true, true ? EngineStore.rpm.toFixed(0) + " RPM" : "") },
                        { label: "Odometer", value: debugScreen.safeVal(true, true ? (EngineStore.odometer / 1000).toFixed(1) + " km" : "") }
                    ]
                }

                // ---- Switches section ----
                DebugSection {
                    sectionTitle: "SWITCHES"
                    entries: [
                        { label: "Kickstand", value: debugScreen.safeVal(true, VehicleStore.kickstand) },
                        { label: "Seatbox Lock", value: debugScreen.safeVal(true, VehicleStore.seatboxLock) },
                        { label: "Brake L", value: debugScreen.safeVal(true, VehicleStore.brakeLeft) },
                        { label: "Brake R", value: debugScreen.safeVal(true, VehicleStore.brakeRight) },
                        { label: "Blinker", value: debugScreen.safeVal(true, VehicleStore.blinkerState) }
                    ]
                }

                // ---- GPS section ----
                DebugSection {
                    sectionTitle: "GPS"
                    entries: [
                        { label: "Latitude", value: debugScreen.safeVal(true, true ? GpsStore.latitude.toFixed(6) : "") },
                        { label: "Longitude", value: debugScreen.safeVal(true, true ? GpsStore.longitude.toFixed(6) : "") },
                        { label: "Altitude", value: debugScreen.safeVal(true, true ? GpsStore.altitude.toFixed(1) + " m" : "") }
                    ]
                }

                // ---- Battery 0 section ----
                DebugSection {
                    sectionTitle: "BATTERY 0"
                    entries: [
                        { label: "Present", value: debugScreen.safeVal(true, Battery0Store.present) },
                        { label: "State", value: debugScreen.safeVal(true, Battery0Store.batteryState) },
                        { label: "Charge", value: debugScreen.safeVal(true, true ? Battery0Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(true, true ? Battery0Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Battery 1 section ----
                DebugSection {
                    sectionTitle: "BATTERY 1"
                    entries: [
                        { label: "Present", value: debugScreen.safeVal(true, Battery1Store.present) },
                        { label: "State", value: debugScreen.safeVal(true, Battery1Store.batteryState) },
                        { label: "Charge", value: debugScreen.safeVal(true, true ? Battery1Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(true, true ? Battery1Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Internet section ----
                DebugSection {
                    sectionTitle: "INTERNET"
                    entries: [
                        { label: "Modem", value: debugScreen.safeVal(true, InternetStore.modemState) },
                        { label: "Status", value: debugScreen.safeVal(true, InternetStore.status) },
                        { label: "Cloud", value: debugScreen.safeVal(true, InternetStore.unuCloud) },
                        { label: "IP", value: debugScreen.safeVal(true, InternetStore.ipAddress) }
                    ]
                }

                // ---- OTA section ----
                DebugSection {
                    sectionTitle: "OTA"
                    entries: [
                        { label: "DBC Status", value: debugScreen.safeVal(true, OtaStore.dbcStatus) },
                        { label: "DBC Download", value: debugScreen.safeVal(true, true ? OtaStore.dbcDownloadProgress + "%" : "") }
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
                id: entryRow
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                color: entryRow.index % 2 === 0 ? "#0A0A0A" : "black"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 4

                    Text {
                        Layout.preferredWidth: parent.width * 0.4
                        text: entryRow.modelData.label
                        color: "#9E9E9E"
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: entryRow.modelData.value
                        color: "white"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
