import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
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

    readonly property bool hasVehicle: typeof vehicleStore !== "undefined"
    readonly property bool hasBat0: typeof battery0Store !== "undefined"
    readonly property bool hasBat1: typeof battery1Store !== "undefined"
    readonly property bool hasNet: typeof internetStore !== "undefined"
    readonly property bool hasModem: typeof modemStore !== "undefined"

    // Enum-typed store properties arrive as ints; show the Redis value the
    // store parsed instead.
    function wire(kind, value) {
        return typeof enumStrings !== "undefined" ? enumStrings[kind](value) : String(value)
    }

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    // Service mode forces dashboard.mode to debug and settings-service
    // reasserts the key on any direct edit, so this screen is where a scooter
    // in service mode stays. Leaving it is only meaningful once the overlay is
    // gone, which is what the 3 s hold below is for.
    readonly property bool serviceActive:
        typeof settingsStore !== "undefined" && settingsStore.serviceActive === "true"

    // Debug mode is entered by writing the mode key, so leaving has to write
    // it back: switching the screen alone would be undone by the next sync.
    function leaveScreen() {
        if (typeof settingsService !== "undefined")
            settingsService.updateMode("speedometer")
        if (typeof screenStore !== "undefined")
            screenStore.setScreen(Scooter.ScreenMode.Cluster)
    }

    // The content is taller than the viewport and the DBC has no touchscreen,
    // so the Flickable needs brake-lever scrolling to be reachable at all.
    //
    // Both directions are taps. Scrolling up used to be an 800 ms hold, which
    // made the way back through three viewports of content cost about as long
    // as reading them. Nothing else on this screen wants a plain right tap.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: typeof menuStore === "undefined" || !menuStore.isOpen
        function onLeftTap() {
            var maxY = Math.max(0, flickable.contentHeight - flickable.height)
            scrollAnim.to = Math.min(flickable.contentY + 120, maxY)
            scrollAnim.restart()
        }
        function onRightTap() {
            scrollAnim.to = Math.max(flickable.contentY - 120, 0)
            scrollAnim.restart()
        }
        // Suppressed in service mode: the write it makes is reasserted, so all
        // it buys is a frame on the cluster and a bounce straight back here.
        function onLeftHold() {
            if (!debugScreen.serviceActive)
                debugScreen.leaveScreen()
        }
        // The 3 s hold, and the only way out of service mode from the
        // handlebars now that the menu does not open on this screen.
        function onLeftBrakeHold() {
            if (debugScreen.serviceActive && typeof settingsService !== "undefined")
                settingsService.disableServiceMode()
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
                        { label: "Kickstand", value: debugScreen.safeVal(debugScreen.hasVehicle, debugScreen.hasVehicle ? debugScreen.wire("kickstand", vehicleStore.kickstand) : "") },
                        { label: "Seatbox Lock", value: debugScreen.safeVal(debugScreen.hasVehicle, debugScreen.hasVehicle ? debugScreen.wire("seatboxLock", vehicleStore.seatboxLock) : "") },
                        { label: "Brake L", value: debugScreen.safeVal(debugScreen.hasVehicle, debugScreen.hasVehicle ? debugScreen.wire("toggle", vehicleStore.brakeLeft) : "") },
                        { label: "Brake R", value: debugScreen.safeVal(debugScreen.hasVehicle, debugScreen.hasVehicle ? debugScreen.wire("toggle", vehicleStore.brakeRight) : "") },
                        { label: "Blinker", value: debugScreen.safeVal(debugScreen.hasVehicle, debugScreen.hasVehicle ? debugScreen.wire("blinkerState", vehicleStore.blinkerState) : "") }
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
                        { label: "State", value: debugScreen.safeVal(debugScreen.hasBat0, debugScreen.hasBat0 ? debugScreen.wire("batteryState", battery0Store.batteryState) : "") },
                        { label: "Charge", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(typeof battery0Store !== "undefined", typeof battery0Store !== "undefined" ? battery0Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Battery 1 section ----
                DebugSection {
                    sectionTitle: "BATTERY 1"
                    entries: [
                        { label: "Present", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.present : "") },
                        { label: "State", value: debugScreen.safeVal(debugScreen.hasBat1, debugScreen.hasBat1 ? debugScreen.wire("batteryState", battery1Store.batteryState) : "") },
                        { label: "Charge", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.charge + "%" : "") },
                        { label: "Voltage", value: debugScreen.safeVal(typeof battery1Store !== "undefined", typeof battery1Store !== "undefined" ? battery1Store.voltage + " mV" : "") }
                    ]
                }

                // ---- Internet section ----
                DebugSection {
                    sectionTitle: "INTERNET"
                    entries: [
                        { label: "Modem", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? debugScreen.wire("modemState", internetStore.modemState) : "") },
                        { label: "Connectivity", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? internetStore.connectivity : "") },
                        { label: "Status", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? debugScreen.wire("connectionStatus", internetStore.status) : "") },
                        { label: "Cloud", value: debugScreen.safeVal(debugScreen.hasNet, debugScreen.hasNet ? debugScreen.wire("connectionStatus", internetStore.unuCloud) : "") },
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

        // Debug can stay up while riding, and InputHandler drops every brake
        // gesture off the parked states, so the bar would advertise levers
        // that do nothing and cost a dense screen 53 px doing it.
        //
        // Two rows either way: the tap row plus whichever of Back and the
        // service-mode exit applies. Reserved so the bar keeps its height as
        // the two scroll hints come and go with the position.
        ControlHints {
            Layout.fillWidth: true
            visible: typeof vehicleStore !== "undefined" && vehicleStore.parked
            // The screen is black in both themes.
            isDark: true
            reservedRows: 2
            leftTap: debugScreen.canScrollDown
                ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll down")
                : ""
            rightTap: debugScreen.canScrollUp
                ? (typeof translations !== "undefined" ? translations.controlScrollUp : "Scroll up")
                : ""
            leftHold: debugScreen.serviceActive ? ""
                    : (typeof translations !== "undefined" ? translations.controlBack : "Back")
            leftHoldLong: debugScreen.serviceActive
                ? (typeof translations !== "undefined"
                   ? translations.controlExitServiceMode : "Exit service mode")
                : ""
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
