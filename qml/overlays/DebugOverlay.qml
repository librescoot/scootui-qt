import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Item {
    id: debugOverlay
    anchors.fill: parent
    z: 50
    visible: typeof dashboardStore !== "undefined" && dashboardStore.debugMode === "overlay"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color panelBg: isDark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.6)
    readonly property color defaultBorder: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.26)
    readonly property color textColor: isDark ? "#FFFFFF" : "#000000"

    // --- Color helpers ---
    function stateColor(st) {
        switch (st) {
            case Scooter.VehicleState.ReadyToDrive: return "#4CAF50"  // green
            case Scooter.VehicleState.StandBy: return "#2196F3"        // blue
            case Scooter.VehicleState.Parked: return "#FF9800"          // orange
            case Scooter.VehicleState.Unknown: case Scooter.VehicleState.Off: return "#9E9E9E" // grey
            case Scooter.VehicleState.Booting: return "#9C27B0"         // purple
            case Scooter.VehicleState.ShuttingDown: return "#F44336"    // red
            case Scooter.VehicleState.Hibernating: return "#3F51B5"     // indigo
            case Scooter.VehicleState.HibernatingImminent: return "#E91E63" // pink
            case Scooter.VehicleState.Suspending: return "#F44336"      // red
            case Scooter.VehicleState.SuspendingImminent: return "#E91E63"  // pink
            case Scooter.VehicleState.Updating: return "#FFEB3B"        // yellow
            case Scooter.VehicleState.WaitingHibernation: case Scooter.VehicleState.WaitingHibernationAdvanced:
                return "#673AB7"                     // deep purple
            case Scooter.VehicleState.WaitingHibernationSeatbox: case Scooter.VehicleState.WaitingSeatbox:
                return "#9C27B0"                     // purple
            case Scooter.VehicleState.WaitingHibernationConfirm:
                return "#311B92"                     // deep purple 900
            default: return "#9E9E9E"
        }
    }

    function gpsStateColor(st) {
        switch (st) {
            case Scooter.GpsState.Off: return "#9E9E9E"
            case Scooter.GpsState.Searching: return "#FFEB3B"
            case Scooter.GpsState.FixEstablished: return "#4CAF50"
            case Scooter.GpsState.Error: return "#F44336"
            default: return "#9E9E9E"
        }
    }

    function batteryChargeColor(charge) {
        if (charge > 70) return "#4CAF50"
        if (charge > 30) return "#FF9800"
        return "#F44336"
    }

    function enumName(enumVal, names) {
        // engineStore.throttle is exposed as a bool while every other toggle
        // here is an enum int. names[true] means names["true"], which is
        // undefined rather than names[1], so booleans need their own mapping.
        // The toggle enums put On first, so true is names[0].
        if (typeof enumVal === "boolean")
            return enumVal ? names[0] : names[1]
        return (enumVal >= 0 && enumVal < names.length) ? names[enumVal] : "?"
    }

    // Display labels indexed by enum int value (ScootEnums order).
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
    function vs(prop) { return typeof vehicleStore !== "undefined" ? vehicleStore[prop] : 0 }
    function es(prop) { return typeof engineStore !== "undefined" ? engineStore[prop] : 0 }
    function gs(prop) { return typeof gpsStore !== "undefined" ? gpsStore[prop] : 0 }
    function is_(prop) { return typeof internetStore !== "undefined" ? internetStore[prop] : 0 }
    function b0(prop) { return typeof battery0Store !== "undefined" ? battery0Store[prop] : 0 }
    function b1(prop) { return typeof battery1Store !== "undefined" ? battery1Store[prop] : 0 }
    function aux(prop) { return typeof auxBatteryStore !== "undefined" ? auxBatteryStore[prop] : 0 }
    function cb(prop) { return typeof cbBatteryStore !== "undefined" ? cbBatteryStore[prop] : 0 }

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
            text: typeof vehicleStore !== "undefined" ? vehicleStore.stateRaw : "?"
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
                    text: enumName(vs("brakeRight"), toggleNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 4. GPS — below blinker/brake panels. Mirrors the full Redis `gps` hash
    // so debugging covers everything modem-service publishes.
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

            Text {
                text: "GPS: " + enumName(gs("gpsState"), gpsStateNames) +
                      "  FIX: " + (gs("fix") || "—") +
                      "  MODE: " + (gs("mode") || "—")
                font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
            }
            Text {
                text: "LAT: " + Number(gs("latitude")).toFixed(5) +
                      "  LON: " + Number(gs("longitude")).toFixed(5)
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "ALT: " + Number(gs("altitude")).toFixed(1) + "m" +
                      "  CRS: " + Number(gs("course")).toFixed(0) + "°" +
                      "  SPD: " + Number(gs("speed")).toFixed(1) + " km/h"
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "SATS: " + gs("satellitesUsed") + "/" + gs("satellitesVisible") +
                      "  SNR: " + Number(gs("snr")).toFixed(1) + " dB"
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "HDOP: " + Number(gs("hdop")).toFixed(1) +
                      "  VDOP: " + Number(gs("vdop")).toFixed(1) +
                      "  PDOP: " + Number(gs("pdop")).toFixed(1)
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "EPH: " + Number(gs("eph")).toFixed(1) + "m" +
                      "  EPT: " + Number(gs("ept")).toFixed(3) + "s" +
                      "  EPS: " + Number(gs("eps")).toFixed(1)
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "TTFF: " + Number(gs("lastTtffSeconds")).toFixed(1) + "s" +
                      " (" + (gs("lastTtffMode") || "—") + ")"
                font.pixelSize: 10; color: debugOverlay.textColor
            }
            Text {
                text: "TS:  " + (gs("timestamp") || "—")
                font.pixelSize: 9; color: "#9E9E9E"
            }
            Text {
                text: "UPD: " + (gs("updated") || "—")
                font.pixelSize: 9; color: "#9E9E9E"
            }
            Text {
                text: "flags:" +
                      " valid:" + (gs("hasValidGps") ? "Y" : "N") +
                      " recent:" + (gs("hasRecentFix") ? "Y" : "N") +
                      " act:" + (gs("active") ? "Y" : "N") +
                      " conn:" + (gs("connected") ? "Y" : "N")
                font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
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
        border.color: is_("status") === Scooter.ConnectionStatus.Connected ? "#2196F3" : defaultBorder

        Column {
            id: inetCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "MODEM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(is_("modemState"), modemStateNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "CLOUD: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: enumName(is_("unuCloud"), connectionStatusNames)
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "SIGNAL: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: is_("signalQuality") + "%"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "TECH: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof internetStore !== "undefined" ? internetStore.accessTech : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 6. Dashboard Info — left column, below GPS
    // =====================================================================
    Rectangle {
        x: 10; y: 250
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
                Text { text: "BRI: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: (typeof dashboardStore !== "undefined" && dashboardStore.brightness >= 0)
                          ? dashboardStore.brightness.toFixed(1) + " lx" : "N/A"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "BLT: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: (typeof dashboardStore !== "undefined" && dashboardStore.backlight >= 0)
                          ? dashboardStore.backlight : "N/A"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "THM: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof settingsStore !== "undefined" ? settingsStore.theme : "N/A"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 7. Engine — left column, below dashboard info
    // =====================================================================
    Rectangle {
        x: 10; y: 290
        width: engCol.width + 20
        height: engCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5
        border.color: defaultBorder

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
                    text: typeof engineStore !== "undefined" ? Math.floor(engineStore.rpm).toString() : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "PWR: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof engineStore !== "undefined"
                          ? (engineStore.motorVoltage * engineStore.motorCurrent / 1000000).toFixed(0) + " W"
                          : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "EBS: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: {
                        var s = enumName(es("kers"), toggleNames)
                        if (typeof engineStore !== "undefined")
                            s += "  " + (engineStore.acceptedRegenVoltage / 1000).toFixed(1) + " V / "
                               + (engineStore.acceptedRegenCurrent / 1000).toFixed(1) + " A"
                        return s
                    }
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "REGEN: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: {
                        if (typeof engineStore === "undefined") return "?"
                        var s = engineStore.regenAvailable ? "yes" : "no"
                        s += " (" + engineStore.regenReason + ")"
                        s += "  " + (engineStore.regenExpected / 1000).toFixed(1) + " A"
                        return s
                    }
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
        }
    }

    // =====================================================================
    // 8. Motor details — right side, alongside engine
    // =====================================================================
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 290
        width: motorCol.width + 20
        height: motorCol.height + 10
        radius: 4; color: panelBg
        border.width: 1.5; border.color: defaultBorder

        Column {
            id: motorCol
            anchors.centerIn: parent
            spacing: 1
            Row {
                spacing: 0
                Text { text: "MOTOR V: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof engineStore !== "undefined"
                          ? (engineStore.motorVoltage / 1000).toFixed(1) + " V" : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "MOTOR I: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof engineStore !== "undefined"
                          ? (engineStore.motorCurrent / 1000).toFixed(1) + " A" : "?"
                    font.pixelSize: 10; font.bold: true; color: debugOverlay.textColor
                }
            }
            Row {
                spacing: 0
                Text { text: "TEMP: "; font.pixelSize: 10; color: "#9E9E9E" }
                Text {
                    text: typeof engineStore !== "undefined"
                          ? engineStore.temperature.toFixed(1) + "\u00B0C" : "?"
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
                property bool present: typeof battery0Store !== "undefined" && battery0Store.present
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
                                  (typeof battery0Store !== "undefined" ? battery0Store.firmwareVersion : "?")
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: parent.parent.present
                        text: "T1:" + b0("temperature0") + " T2:" + b0("temperature1") +
                              " T3:" + b0("temperature2") + " T4:" + b0("temperature3") + "°C"
                        font.pixelSize: 9; color: "#9E9E9E"
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
                property bool present: typeof battery1Store !== "undefined" && battery1Store.present
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
                                  (typeof battery1Store !== "undefined" ? battery1Store.firmwareVersion : "?")
                            font.pixelSize: 9; color: "#9E9E9E"
                        }
                    }
                    Text {
                        visible: parent.parent.present
                        text: "T1:" + b1("temperature0") + " T2:" + b1("temperature1") +
                              " T3:" + b1("temperature2") + " T4:" + b1("temperature3") + "°C"
                        font.pixelSize: 9; color: "#9E9E9E"
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
                        text: enumName(cb("chargeStatus"), ["Charging", "NotCharging", "Unknown"]) +
                              " / SoH " + cb("stateOfHealth") + "% / " + cb("temperature") + "°C"
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
