import QtQuick
import QtQuick.Layouts
import ScootUI

Item {
    id: debugOverlay
    anchors.fill: parent
    z: 50
    visible: DashboardStore.debugMode === "overlay"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color panelBg: isDark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.6)
    readonly property color defaultBorder: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.26)
    readonly property color textColor: isDark ? "#FFFFFF" : "#000000"

    // --- Enum int values (must match Enums.h order) ---
    // ScooterState
    readonly property int stUnknown: 0
    readonly property int stStandBy: 1
    readonly property int stReadyToDrive: 2
    readonly property int stOff: 3
    readonly property int stParked: 4
    readonly property int stBooting: 5
    readonly property int stShuttingDown: 6
    readonly property int stHibernating: 7
    readonly property int stHibernatingImminent: 8
    readonly property int stSuspending: 9
    readonly property int stSuspendingImminent: 10
    readonly property int stUpdating: 11
    readonly property int stWaitingSeatbox: 12
    readonly property int stWaitingHibernation: 13
    readonly property int stWaitingHibernationAdv: 14
    readonly property int stWaitingHibernationSeatbox: 15
    readonly property int stWaitingHibernationConfirm: 16

    // GpsState
    readonly property int gpsOff: 0
    readonly property int gpsSearching: 1
    readonly property int gpsFix: 2
    readonly property int gpsError: 3

    // Toggle
    readonly property int toggleOn: 0
    readonly property int toggleOff: 1

    // ConnectionStatus
    readonly property int csConnected: 0

    // ModemState
    readonly property int msConnected: 2

    // BlinkerSwitch / BlinkerState
    readonly property int blOff: 0
    readonly property int blLeft: 1
    readonly property int blRight: 2
    readonly property int blBoth: 3

    // BatteryState
    readonly property int bsUnknown: 0
    readonly property int bsAsleep: 1
    readonly property int bsIdle: 2
    readonly property int bsActive: 3

    // --- Color helpers ---
    function stateColor(st) {
        switch (st) {
            case stReadyToDrive: return "#4CAF50"  // green
            case stStandBy: return "#2196F3"        // blue
            case stParked: return "#FF9800"          // orange
            case stUnknown: case stOff: return "#9E9E9E" // grey
            case stBooting: return "#9C27B0"         // purple
            case stShuttingDown: return "#F44336"    // red
            case stHibernating: return "#3F51B5"     // indigo
            case stHibernatingImminent: return "#E91E63" // pink
            case stSuspending: return "#F44336"      // red
            case stSuspendingImminent: return "#E91E63"  // pink
            case stUpdating: return "#FFEB3B"        // yellow
            case stWaitingHibernation: case stWaitingHibernationAdv:
                return "#673AB7"                     // deep purple
            case stWaitingHibernationSeatbox: case stWaitingSeatbox:
                return "#9C27B0"                     // purple
            case stWaitingHibernationConfirm:
                return "#311B92"                     // deep purple 900
            default: return "#9E9E9E"
        }
    }

    function gpsStateColor(st) {
        switch (st) {
            case gpsOff: return "#9E9E9E"
            case gpsSearching: return "#FFEB3B"
            case gpsFix: return "#4CAF50"
            case gpsError: return "#F44336"
            default: return "#9E9E9E"
        }
    }

    function batteryChargeColor(charge) {
        if (charge > 70) return "#4CAF50"
        if (charge > 30) return "#FF9800"
        return "#F44336"
    }

    function enumName(enumVal, names) {
        return (enumVal >= 0 && enumVal < names.length) ? names[enumVal] : "?"
    }

    readonly property var scooterStateNames: [
        "Unknown", "StandBy", "ReadyToDrive", "Off", "Parked",
        "Booting", "ShuttingDown", "Hibernating", "HibernatingImminent",
        "Suspending", "SuspendingImminent", "Updating",
        "WaitingSeatbox", "WaitingHibernation", "WaitingHibernationAdvanced",
        "WaitingHibernationSeatbox", "WaitingHibernationConfirm"
    ]
    readonly property var gpsStateNames: ["Off", "Searching", "FixEstablished", "Error"]
    readonly property var toggleNames: ["On", "Off"]
    readonly property var blinkerSwitchNames: ["Off", "Left", "Right"]
    readonly property var blinkerStateNames: ["Off", "Left", "Right", "Both"]
    readonly property var connectionStatusNames: ["Connected", "Disconnected"]
    readonly property var modemStateNames: ["Off", "Disconnected", "Connected"]
    readonly property var batteryStateNames: ["Unknown", "Asleep", "Idle", "Active"]
    readonly property var auxChargeStatusNames: ["NotCharging", "FloatCharge", "AbsorptionCharge", "BulkCharge"]

    // Safe accessors
    function vs(prop) { return VehicleStore[prop]}
    function es(prop) { return EngineStore[prop]}
    function gs(prop) { return GpsStore[prop]}
    function is_(prop) { return InternetStore[prop]}
    function b0(prop) { return Battery0Store[prop]}
    function b1(prop) { return Battery1Store[prop]}
    function aux(prop) { return AuxBatteryStore[prop]}
    function cb(prop) { return CbBatteryStore[prop]}

    // =====================================================================
    // 1. Vehicle State — centered, below status bar
    // =====================================================================
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 160
        width: vehStateText.width + 20
        height: vehStateText.height + 10
        radius: 4
        color: panelBg
        border.width: 1.5
        border.color: stateColor(vs("state"))

        Text {
            id: vehStateText
            anchors.centerIn: parent
            text: VehicleStore.stateRaw
            font.pixelSize: 12
            font.bold: true
            color: debugOverlay.textColor
        }
    }

    // =====================================================================
    // 2. Left Blinker/Brake — below status bar
    // =====================================================================
    Rectangle {
        x: 60; y: 50
        width: leftBlinkCol.width + 20
        height: leftBlinkCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: defaultBorder

        Column {
            id: leftBlinkCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "BLINK: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(vs("blinkerSwitch"), blinkerSwitchNames) + "/" +
                          enumName(vs("blinkerState"), blinkerStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "BRAKE: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(vs("brakeLeft"), toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 3. Right Blinker/Brake — below status bar
    // =====================================================================
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 60
        y: 50
        width: rightBlinkCol.width + 20
        height: rightBlinkCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: defaultBorder

        Column {
            id: rightBlinkCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: enumName(vs("blinkerSwitch"), blinkerSwitchNames) + "/" +
                          enumName(vs("blinkerState"), blinkerStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :BLINK"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: enumName(vs("brakeRight"), toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :BRAKE"; font.pixelSize: 10; color: "#9E9E9E" }
            }
        }
    }

    // =====================================================================
    // 4. GPS — below blinker/brake panels
    // =====================================================================
    Rectangle {
        x: 10; y: 100
        width: gpsCol.width + 20
        height: gpsCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: gpsStateColor(gs("gpsState"))

        Column {
            id: gpsCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "GPS: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(gs("gpsState"), gpsStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "LAT: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: GpsStore.latitude.toFixed(5)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "LON: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: GpsStore.longitude.toFixed(5)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "SPD: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: GpsStore.speed.toFixed(1) + " km/h"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 5. Internet — below blinker/brake panels
    // =====================================================================
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 100
        width: inetCol.width + 20
        height: inetCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5
        border.color: is_("status") === csConnected ? "#2196F3" : defaultBorder

        Column {
            id: inetCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: enumName(is_("modemState"), modemStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :MODEM"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: enumName(is_("unuCloud"), connectionStatusNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :CLOUD"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: is_("signalQuality") + "%"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :SIGNAL"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: InternetStore.accessTech
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :TECH"; font.pixelSize: 10; color: "#9E9E9E" }
            }
        }
    }

    // =====================================================================
    // 6. Dashboard Info — left column, below GPS
    // =====================================================================
    Rectangle {
        x: 10; y: 220
        width: dashCol.width + 20
        height: dashCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: defaultBorder

        Column {
            id: dashCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "THM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: SettingsStore.theme
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 7. Engine — left column, below dashboard info
    // =====================================================================
    Rectangle {
        x: 10; y: 260
        width: engCol.width + 20
        height: engCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5
        border.color: es("powerState") === toggleOn ? "#2196F3" : defaultBorder

        Column {
            id: engCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "THR: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(es("throttle"), toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "RPM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: Math.floor(EngineStore.rpm).toString()
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "PWR: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: (EngineStore.motorVoltage * EngineStore.motorCurrent / 1000000).toFixed(0) + " W"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "EBS: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(es("kers"), toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 8. Motor details — top: 260, right: 10
    // =====================================================================
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 260
        width: motorCol.width + 20
        height: motorCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: defaultBorder

        Column {
            id: motorCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: (EngineStore.motorVoltage / 1000).toFixed(1) + " V"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :MOTOR V"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: (EngineStore.motorCurrent / 1000).toFixed(1) + " A"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :MOTOR I"; font.pixelSize: 10; color: "#9E9E9E" }
            }
            Row {
                layoutDirection: Qt.RightToLeft; spacing: 0
                Text {
                    text: EngineStore.temperature.toFixed(1) + "\u00B0C"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
                Text { text: " :TEMP"; font.pixelSize: 10; color: "#9E9E9E" }
            }
        }
    }

    // =====================================================================
    // 9. Battery grid — bottom: 0, centered (2x2)
    // =====================================================================
    Column {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4

        // Top row: Main batteries (B0, B1)
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            // Battery 0
            Rectangle {
                property bool present: Battery0Store.present
                property int charge: b0("charge")
                width: b0Col.width + 20
                height: b0Col.height + 10
                radius: 4; color: panelBg
                border.width: 1.5
                border.color: present ? batteryChargeColor(charge) : "#9E9E9E"

                Column {
                    id: b0Col
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        visible: parent.parent.present
                        text: "B0: " + b0("charge") + "% " +
                              (b0("voltage") / 1000).toFixed(1) + "V " +
                              (b0("current") / 1000).toFixed(1) + "A"
                        font.pixelSize: 10; font.bold: true
                        color: parent.parent.present ? batteryChargeColor(parent.parent.charge) : "#9E9E9E"
                    }
                    Row {
                        visible: parent.parent.present
                        spacing: 0
                        Text {
                            text: enumName(b0("batteryState"), batteryStateNames)
                            font.pixelSize: 9; font.bold: true; color: debugOverlay.textColor
                        }
                        Text {
                            text: " - " + b0("cycleCount") + " cyc - fw " +
                                  Battery0Store.firmwareVersion
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: !parent.parent.present
                        text: "B0: --"
                        font.pixelSize: 10; font.bold: true; color: "#9E9E9E"
                    }
                }
            }

            // Battery 1
            Rectangle {
                property bool present: Battery1Store.present
                property int charge: b1("charge")
                width: b1Col.width + 20
                height: b1Col.height + 10
                radius: 4; color: panelBg
                border.width: 1.5
                border.color: present ? batteryChargeColor(charge) : "#9E9E9E"

                Column {
                    id: b1Col
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        visible: parent.parent.present
                        text: "B1: " + b1("charge") + "% " +
                              (b1("voltage") / 1000).toFixed(1) + "V " +
                              (b1("current") / 1000).toFixed(1) + "A"
                        font.pixelSize: 10; font.bold: true
                        color: parent.parent.present ? batteryChargeColor(parent.parent.charge) : "#9E9E9E"
                    }
                    Row {
                        visible: parent.parent.present
                        spacing: 0
                        Text {
                            text: enumName(b1("batteryState"), batteryStateNames)
                            font.pixelSize: 9; font.bold: true; color: debugOverlay.textColor
                        }
                        Text {
                            text: " - " + b1("cycleCount") + " cyc - fw " +
                                  Battery1Store.firmwareVersion
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: !parent.parent.present
                        text: "B1: --"
                        font.pixelSize: 10; font.bold: true; color: "#9E9E9E"
                    }
                }
            }
        }

        // Bottom row: CB and AUX batteries
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            // CB Battery
            Rectangle {
                width: cbCol.width + 20
                height: cbCol.height + 10
                radius: 4; color: panelBg
                border.width: 1.5; border.color: defaultBorder

                Column {
                    id: cbCol
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        text: "CBB " + cb("charge") + "% " +
                              (cb("cellVoltage") / 1000000).toFixed(2) + "V " +
                              (cb("current") / 1000).toFixed(2) + "mA"
                        font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                    }
                    Text {
                        text: enumName(cb("chargeStatus"), ["Charging", "NotCharging"]) +
                              " / SoH " + cb("stateOfHealth") + "%"
                        font.pixelSize: 9; color: "#9E9E9E"
                    }
                }
            }

            // AUX Battery
            Rectangle {
                width: auxCol.width + 20
                height: auxCol.height + 10
                radius: 4; color: panelBg
                border.width: 1.5; border.color: defaultBorder

                Column {
                    id: auxCol
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        text: "AUX " + aux("charge") + "% " +
                              (aux("voltage") / 1000).toFixed(1) + "V"
                        font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                    }
                    Text {
                        text: enumName(aux("chargeStatus"), auxChargeStatusNames)
                        font.pixelSize: 9; color: "#9E9E9E"
                    }
                }
            }
        }
    }
}
