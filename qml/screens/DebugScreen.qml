import QtQuick
import QtQuick.Layouts
import "../widgets/components"

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
        if (!storeAvailable)
            return fallback || "N/A";
        if (value === undefined || value === null || value === "")
            return "-";
        return String(value);
    }

    readonly property bool hasNet: typeof internetStore !== "undefined"
    readonly property bool hasModem: typeof modemStore !== "undefined"

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    // Debug mode is entered by writing the mode key, so leaving has to write
    // it back: switching the screen alone would be undone by the next sync.
    function leaveScreen() {
        if (typeof settingsService !== "undefined")
            settingsService.updateMode("speedometer")
        if (typeof screenStore !== "undefined")
            screenStore.setScreen(0)
    }

    // The content is taller than the viewport and the DBC has no touchscreen,
    // so the Flickable needs brake-lever scrolling to be reachable at all.
    // Left double-tap still opens the menu (handled in Main.qml); the menu
    // takes over the levers while it is open.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: typeof menuStore === "undefined" || !menuStore.isOpen
        function onLeftTap() {
            var maxY = Math.max(0, flickable.contentHeight - flickable.height)
            scrollAnim.to = Math.min(flickable.contentY + 120, maxY)
            scrollAnim.restart()
        }
        function onLeftHold() { debugScreen.leaveScreen() }
        function onRightHold() {
            scrollAnim.to = Math.max(flickable.contentY - 120, 0)
            scrollAnim.restart()
        }
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

            NumberAnimation {
                id: scrollAnim
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

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
                        { label: "Modem", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.modemState : "") },
                        { label: "Connectivity", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.connectivity : "") },
                        { label: "Status", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.status : "") },
                        { label: "Cloud", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.unuCloud : "") },
                        { label: "IP", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.ipAddress : "") },
                        { label: "Access Tech", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.accessTech : "") },
                        { label: "Signal", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.signalQuality + "%" : "") },
                        { label: "Health", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.modemHealth : "") }
                    ]
                }

                // ---- Modem identity section ----
                // IMEI and ICCID are what connectivity onboarding asks for, so
                // they get their own section rather than hiding among the
                // connection state above.
                DebugSection {
                    sectionTitle: "SIM / MODEM IDENTITY"
                    entries: [
                        { label: "IMEI", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.simImei : "") },
                        { label: "ICCID", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.simIccid : "") },
                        { label: "IMSI", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.simImsi : "") }
                    ]
                }

                // ---- Modem section ----
                DebugSection {
                    sectionTitle: "MODEM"
                    entries: [
                        { label: "Operator", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.operatorName : "") },
                        { label: "Operator Code", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.operatorCode : "") },
                        { label: "Registration", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.registration : "") },
                        { label: "Roaming", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.isRoaming : "") },
                        { label: "Power", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.powerState : "") },
                        { label: "SIM State", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.simState : "") },
                        { label: "SIM Lock", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.simLock : "") },
                        { label: "PIN Action", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.pinAction : "") },
                        { label: "APN Action", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.apnAction : "") },
                        { label: "Reg Fail", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.registrationFail : "") },
                        { label: "Error", value: debugScreen.safeVal(debugScreen.hasModem, debugScreen.hasModem ? modemStore.errorState : "") }
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

        // Debug can stay up while riding, and InputHandler drops every brake
        // gesture off the parked states, so the bar would advertise levers
        // that do nothing and cost a dense screen 53 px doing it.
        ControlHints {
            Layout.fillWidth: true
            visible: typeof vehicleStore !== "undefined" && vehicleStore.parked
            reservedRows: 2
            leftTap: debugScreen.canScrollDown
                ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                : ""
            leftHold: typeof translations !== "undefined"
                      ? translations.controlBack : "Back"
            rightHold: debugScreen.canScrollUp
                ? (typeof translations !== "undefined" ? translations.controlScrollUp : "Scroll up")
                : ""
        }
    }
}
