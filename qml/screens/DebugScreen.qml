import QtQuick
import QtQuick.Layouts
import ScootUI

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
                        { label: "State", value: String(VehicleStore.stateRaw) }
                    ]
                }

                // ---- Engine section ----
                DebugSection {
                    sectionTitle: "ENGINE"
                    entries: [
                        { label: "Speed", value: EngineStore.speed.toFixed(1) + " km/h" },
                        { label: "RPM", value: EngineStore.rpm.toFixed(0) + " RPM" },
                        { label: "Odometer", value: (EngineStore.odometer / 1000).toFixed(1) + " km" }
                    ]
                }

                // ---- Switches section ----
                DebugSection {
                    sectionTitle: "SWITCHES"
                    entries: [
                        { label: "Kickstand", value: String(VehicleStore.kickstand) },
                        { label: "Seatbox Lock", value: String(VehicleStore.seatboxLock) },
                        { label: "Brake L", value: String(VehicleStore.brakeLeft) },
                        { label: "Brake R", value: String(VehicleStore.brakeRight) },
                        { label: "Blinker", value: String(VehicleStore.blinkerState) }
                    ]
                }

                // ---- GPS section ----
                DebugSection {
                    sectionTitle: "GPS"
                    entries: [
                        { label: "Latitude", value: GpsStore.latitude.toFixed(6) },
                        { label: "Longitude", value: GpsStore.longitude.toFixed(6) },
                        { label: "Altitude", value: GpsStore.altitude.toFixed(1) + " m" }
                    ]
                }

                // ---- Battery 0 section ----
                DebugSection {
                    sectionTitle: "BATTERY 0"
                    entries: [
                        { label: "Present", value: String(Battery0Store.present) },
                        { label: "State", value: String(Battery0Store.batteryState) },
                        { label: "Charge", value: Battery0Store.charge + "%" },
                        { label: "Voltage", value: Battery0Store.voltage + " mV" }
                    ]
                }

                // ---- Battery 1 section ----
                DebugSection {
                    sectionTitle: "BATTERY 1"
                    entries: [
                        { label: "Present", value: String(Battery1Store.present) },
                        { label: "State", value: String(Battery1Store.batteryState) },
                        { label: "Charge", value: Battery1Store.charge + "%" },
                        { label: "Voltage", value: Battery1Store.voltage + " mV" }
                    ]
                }

                // ---- Internet section ----
                DebugSection {
                    sectionTitle: "INTERNET"
                    entries: [
                        { label: "Modem", value: String(InternetStore.modemState) },
                        { label: "Status", value: String(InternetStore.status) },
                        { label: "Cloud", value: String(InternetStore.unuCloud) },
                        { label: "IP", value: String(InternetStore.ipAddress) }
                    ]
                }

                // ---- OTA section ----
                DebugSection {
                    sectionTitle: "OTA"
                    entries: [
                        { label: "DBC Status", value: String(OtaStore.dbcStatus) },
                        { label: "DBC Download", value: OtaStore.dbcDownloadProgress + "%" }
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
