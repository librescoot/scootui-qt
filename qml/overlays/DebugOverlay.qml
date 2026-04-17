import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Item {
    id: debugOverlay
    anchors.fill: parent
    z: 50
    visible: DashboardStore.debugMode === "overlay"

    readonly property bool isDark: ThemeStore.isDark
    readonly property color panelBg: debugOverlay.isDark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.6)
    readonly property color defaultBorder: debugOverlay.isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.26)
    readonly property color textColor: debugOverlay.isDark ? "#FFFFFF" : "#000000"

    // --- Enum int values (must match Enums.h order) ---
    // VehicleState
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
            case debugOverlay.stReadyToDrive: return "#4CAF50"  // green
            case debugOverlay.stStandBy: return "#2196F3"        // blue
            case debugOverlay.stParked: return "#FF9800"          // orange
            case debugOverlay.stUnknown: case debugOverlay.stOff: return "#9E9E9E" // grey
            case debugOverlay.stBooting: return "#9C27B0"         // purple
            case debugOverlay.stShuttingDown: return "#F44336"    // red
            case debugOverlay.stHibernating: return "#3F51B5"     // indigo
            case debugOverlay.stHibernatingImminent: return "#E91E63" // pink
            case debugOverlay.stSuspending: return "#F44336"      // red
            case debugOverlay.stSuspendingImminent: return "#E91E63"  // pink
            case debugOverlay.stUpdating: return "#FFEB3B"        // yellow
            case debugOverlay.stWaitingHibernation: case debugOverlay.stWaitingHibernationAdv:
                return "#673AB7"                     // deep purple
            case debugOverlay.stWaitingHibernationSeatbox: case debugOverlay.stWaitingSeatbox:
                return "#9C27B0"                     // purple
            case debugOverlay.stWaitingHibernationConfirm:
                return "#311B92"                     // deep purple 900
            default: return "#9E9E9E"
        }
    }

    function gpsStateColor(st) {
        switch (st) {
            case debugOverlay.gpsOff: return "#9E9E9E"
            case debugOverlay.gpsSearching: return "#FFEB3B"
            case debugOverlay.gpsFix: return "#4CAF50"
            case debugOverlay.gpsError: return "#F44336"
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
    function vs(prop) { return true ? VehicleStore[prop] : 0 }
    function es(prop) { return true ? EngineStore[prop] : 0 }
    function gs(prop) { return true ? GpsStore[prop] : 0 }
    function is_(prop) { return true ? InternetStore[prop] : 0 }
    function b0(prop) { return true ? Battery0Store[prop] : 0 }
    function b1(prop) { return true ? Battery1Store[prop] : 0 }
    function aux(prop) { return true ? AuxBatteryStore[prop] : 0 }
    function cb(prop) { return true ? CbBatteryStore[prop] : 0 }

    // =====================================================================
    // 1. Vehicle State — centered, below status bar
    // =====================================================================
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 160
        width: vehStateText.width + 20
        height: vehStateText.height + 10
        radius: 4
        color: debugOverlay.panelBg
        border.width: 1.5
        border.color: debugOverlay.stateColor(debugOverlay.vs("state"))

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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5; border.color: debugOverlay.defaultBorder

        Column {
            id: leftBlinkCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "BLINK: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.vs("blinkerSwitch"), debugOverlay.blinkerSwitchNames) + "/" +
                          debugOverlay.enumName(debugOverlay.vs("blinkerState"), debugOverlay.blinkerStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "BRAKE: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.vs("brakeLeft"), debugOverlay.toggleNames)
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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5; border.color: debugOverlay.defaultBorder

        Column {
            id: rightBlinkCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "BLINK: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.vs("blinkerSwitch"), debugOverlay.blinkerSwitchNames) + "/" +
                          debugOverlay.enumName(debugOverlay.vs("blinkerState"), debugOverlay.blinkerStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "BRAKE: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.vs("brakeRight"), debugOverlay.toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5; border.color: debugOverlay.gpsStateColor(debugOverlay.gs("gpsState"))

        Column {
            id: gpsCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "GPS: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.gs("gpsState"), debugOverlay.gpsStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "LAT: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true ? GpsStore.latitude.toFixed(5) : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "LON: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true ? GpsStore.longitude.toFixed(5) : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "SPD: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: (true ? GpsStore.speed.toFixed(1) : "?") + " km/h"
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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5
        border.color: debugOverlay.is_("status") === debugOverlay.csConnected ? "#2196F3" : debugOverlay.defaultBorder

        Column {
            id: inetCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "MODEM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.is_("modemState"), debugOverlay.modemStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "CLOUD: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.is_("unuCloud"), debugOverlay.connectionStatusNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "SIGNAL: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.is_("signalQuality") + "%"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "TECH: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: InternetStore.accessTech
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5; border.color: debugOverlay.defaultBorder

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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5
        border.color: debugOverlay.es("powerState") === debugOverlay.toggleOn ? "#2196F3" : debugOverlay.defaultBorder

        Column {
            id: engCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "THR: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.es("throttle"), debugOverlay.toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "RPM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true ? Math.floor(EngineStore.rpm).toString() : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "PWR: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true
                          ? (EngineStore.motorVoltage * EngineStore.motorCurrent / 1000000).toFixed(0) + " W"
                          : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "EBS: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: debugOverlay.enumName(debugOverlay.es("kers"), debugOverlay.toggleNames)
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
        radius: 4; color: debugOverlay.panelBg
        border.width: 1.5; border.color: debugOverlay.defaultBorder

        Column {
            id: motorCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "MOTOR V: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true
                          ? (EngineStore.motorVoltage / 1000).toFixed(1) + " V" : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "MOTOR I: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true
                          ? (EngineStore.motorCurrent / 1000).toFixed(1) + " A" : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "TEMP: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: true
                          ? EngineStore.temperature.toFixed(1) + "\u00B0C" : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
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
                id: batt0Card
                property bool present: Battery0Store.present
                property int charge: debugOverlay.b0("charge")
                width: b0Col.width + 20
                height: b0Col.height + 10
                radius: 4; color: debugOverlay.panelBg
                border.width: 1.5
                border.color: batt0Card.present ? debugOverlay.batteryChargeColor(batt0Card.charge) : "#9E9E9E"

                Column {
                    id: b0Col
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        visible: batt0Card.present
                        text: "B0: " + debugOverlay.b0("charge") + "% " +
                              (debugOverlay.b0("voltage") / 1000).toFixed(1) + "V " +
                              (debugOverlay.b0("current") / 1000).toFixed(1) + "A"
                        font.pixelSize: 10; font.bold: true
                        color: batt0Card.present ? debugOverlay.batteryChargeColor(batt0Card.charge) : "#9E9E9E"
                    }
                    Row {
                        visible: batt0Card.present
                        spacing: 0
                        Text {
                            text: debugOverlay.enumName(debugOverlay.b0("batteryState"), debugOverlay.batteryStateNames)
                            font.pixelSize: 9; font.bold: true; color: debugOverlay.textColor
                        }
                        Text {
                            text: " - " + debugOverlay.b0("cycleCount") + " cyc - fw " +
                                  (Battery0Store.firmwareVersion)
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: !batt0Card.present
                        text: "B0: --"
                        font.pixelSize: 10; font.bold: true; color: "#9E9E9E"
                    }
                }
            }

            // Battery 1
            Rectangle {
                id: batt1Card
                property bool present: Battery1Store.present
                property int charge: debugOverlay.b1("charge")
                width: b1Col.width + 20
                height: b1Col.height + 10
                radius: 4; color: debugOverlay.panelBg
                border.width: 1.5
                border.color: batt1Card.present ? debugOverlay.batteryChargeColor(batt1Card.charge) : "#9E9E9E"

                Column {
                    id: b1Col
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        visible: batt1Card.present
                        text: "B1: " + debugOverlay.b1("charge") + "% " +
                              (debugOverlay.b1("voltage") / 1000).toFixed(1) + "V " +
                              (debugOverlay.b1("current") / 1000).toFixed(1) + "A"
                        font.pixelSize: 10; font.bold: true
                        color: batt1Card.present ? debugOverlay.batteryChargeColor(batt1Card.charge) : "#9E9E9E"
                    }
                    Row {
                        visible: batt1Card.present
                        spacing: 0
                        Text {
                            text: debugOverlay.enumName(debugOverlay.b1("batteryState"), debugOverlay.batteryStateNames)
                            font.pixelSize: 9; font.bold: true; color: debugOverlay.textColor
                        }
                        Text {
                            text: " - " + debugOverlay.b1("cycleCount") + " cyc - fw " +
                                  (Battery1Store.firmwareVersion)
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: !batt1Card.present
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
                radius: 4; color: debugOverlay.panelBg
                border.width: 1.5; border.color: debugOverlay.defaultBorder

                Column {
                    id: cbCol
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        text: "CBB " + debugOverlay.cb("charge") + "% " +
                              (debugOverlay.cb("cellVoltage") / 1000000).toFixed(2) + "V " +
                              (debugOverlay.cb("current") / 1000).toFixed(2) + "mA"
                        font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                    }
                    Text {
                        text: debugOverlay.enumName(debugOverlay.cb("chargeStatus"), ["Charging", "NotCharging"]) +
                              " / SoH " + debugOverlay.cb("stateOfHealth") + "%"
                        font.pixelSize: 9; color: "#9E9E9E"
                    }
                }
            }

            // AUX Battery
            Rectangle {
                width: auxCol.width + 20
                height: auxCol.height + 10
                radius: 4; color: debugOverlay.panelBg
                border.width: 1.5; border.color: debugOverlay.defaultBorder

                Column {
                    id: auxCol
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        text: "AUX " + debugOverlay.aux("charge") + "% " +
                              (debugOverlay.aux("voltage") / 1000).toFixed(1) + "V"
                        font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                    }
                    Text {
                        text: debugOverlay.enumName(debugOverlay.aux("chargeStatus"), debugOverlay.auxChargeStatusNames)
                        font.pixelSize: 9; color: "#9E9E9E"
                    }
                }
            }
        }
    }
}
