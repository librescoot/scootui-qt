import QtQuick
import QtQuick.Layouts
import ScootUI 1.0
import "../widgets/components"

Rectangle {
    id: root
    color: "black"
    property int tabIndex: 0
    readonly property var tabs: ["Overview", "Power", "Batteries", "Vehicle", "GPS", "Network", "Modem", "Software", "Updates"]
    readonly property var aux: typeof auxBatteryStore !== "undefined" ? auxBatteryStore : null
    readonly property var cbb: typeof cbBatteryStore !== "undefined" ? cbBatteryStore : null
    readonly property var bat0: typeof battery0Store !== "undefined" ? battery0Store : null
    readonly property var bat1: typeof battery1Store !== "undefined" ? battery1Store : null
    readonly property var vehicle: typeof vehicleStore !== "undefined" ? vehicleStore : null
    readonly property var engine: typeof engineStore !== "undefined" ? engineStore : null
    readonly property var gps: typeof gpsStore !== "undefined" ? gpsStore : null
    readonly property var net: typeof internetStore !== "undefined" ? internetStore : null
    readonly property var modem: typeof modemStore !== "undefined" ? modemStore : null
    readonly property var ota: typeof otaStore !== "undefined" ? otaStore : null
    readonly property var info: typeof systemInfoService !== "undefined" ? systemInfoService : null
    readonly property bool serviceActive: typeof settingsStore !== "undefined" && settingsStore.serviceActive === "true"
    readonly property color muted: "#bdbdbd"
    readonly property color good: "#78d582"
    readonly property int fontSize: typeof themeStore !== "undefined" ? Math.max(17, themeStore.fontBody) : 17
    readonly property string fontFamily: "Roboto"
    readonly property bool canScroll: flick.contentHeight > flick.height + 1

    Component.onCompleted: if (info) info.loadVersions()
    function val(v) { return v === undefined || v === null || v === "" ? "-" : String(v) }
    function wire(kind, v) { return typeof enumStrings !== "undefined" ? val(enumStrings[kind](v)) : "-" }
    function pack(p, value) { return p && p.present ? val(value) : "-" }
    function voltage(v) { return (v / 1000).toFixed(1) + " V" }
    function current(v) { return (v / 1000).toFixed(1) + " A" }
    function temp(p) {
        if (!p || !p.present) return "-"
        var a = [p.temperature0, p.temperature1, p.temperature2, p.temperature3]
        return Math.min.apply(null, a) + "-" + Math.max.apply(null, a) + " °C"
    }
    function auxCharge() { return !aux || !aux.chargeValid ? "-" : aux.charge === 255 ? "Error" : aux.charge + "%" }
    function auxStage() { return aux && aux.chargeStatusValid ? wire("auxChargeStatus", aux.chargeStatus) : "-" }
    function cbbStage() { return cbb && cbb.present ? wire("chargeStatus", cbb.chargeStatus) : "-" }
    function packState(p) { return !p ? "-" : !p.present ? "Not present" : wire("batteryState", p.batteryState) }
    function gpsSummary() {
        if (!gps) return "GPS -"
        switch (gps.gpsState) {
        case Scooter.GpsState.FixEstablished: return "GPS Fix · " + gps.satellitesUsed + " sats"
        case Scooter.GpsState.Searching: return "GPS searching"
        case Scooter.GpsState.Off: return "GPS off"
        case Scooter.GpsState.Error: return "GPS error"
        default: return "GPS unknown"
        }
    }
    function lteSummary() {
        if (!net) return "LTE -"
        switch (net.modemState) {
        case Scooter.ModemState.Connected: return "LTE " + net.signalQuality + "%"
        case Scooter.ModemState.Disconnected: return "LTE disconnected"
        case Scooter.ModemState.Off: return "LTE off"
        default: return "LTE unknown"
        }
    }
    function row(label, value) { return { label: label, value: val(value) } }
    function compareRow(label, a, b) { return { label: label, left: val(a), right: val(b) } }
    function changeTab(delta) { tabIndex = (tabIndex + delta + tabs.length) % tabs.length; flick.contentY = 0 }
    function leaveScreen() {
        if (typeof settingsService !== "undefined") settingsService.updateMode("speedometer")
        if (typeof screenStore !== "undefined") screenStore.setScreen(Scooter.ScreenMode.Cluster)
    }
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: root.visible && (typeof menuStore === "undefined" || !menuStore.isOpen)
        function onLeftTap() { root.changeTab(1) }
        function onRightTap() { root.changeTab(-1) }
        function onRightHold() {
            if (root.canScroll)
                flick.contentY = flick.contentY + flick.height >= flick.contentHeight - 1
                    ? 0 : Math.min(flick.contentY + flick.height - 32, flick.contentHeight - flick.height)
        }
        function onLeftHold() { if (!root.serviceActive) root.leaveScreen() }
        function onLeftBrakeHold() {
            if (root.serviceActive && typeof settingsService !== "undefined") settingsService.disableServiceMode()
        }
    }

    readonly property var comparison: [
        compareRow("Voltage", pack(bat0, bat0 ? voltage(bat0.voltage) : ""), pack(bat1, bat1 ? voltage(bat1.voltage) : "")),
        compareRow("Current", pack(bat0, bat0 ? current(bat0.current) : ""), pack(bat1, bat1 ? current(bat1.current) : "")),
        compareRow("Cycles", pack(bat0, bat0 ? bat0.cycleCount : ""), pack(bat1, bat1 ? bat1.cycleCount : "")),
        compareRow("Health", pack(bat0, bat0 ? bat0.stateOfHealth + "%" : ""), pack(bat1, bat1 ? bat1.stateOfHealth + "%" : "")),
        compareRow("Temp", temp(bat0), temp(bat1)),
        compareRow("BMS ver.", pack(bat0, bat0 ? bat0.firmwareVersion : ""), pack(bat1, bat1 ? bat1.firmwareVersion : "")),
        compareRow("Serial", pack(bat0, bat0 ? bat0.serialNumber : ""), pack(bat1, bat1 ? bat1.serialNumber : ""))
    ]
    readonly property var powerRows: [
        row("AUX voltage", aux && aux.voltageValid ? (aux.voltage / 1000).toFixed(2) + " V" : "-"),
        row("AUX estimate", auxCharge()), row("AUX stage", auxStage()),
        row("CBB state", cbbStage()), row("CBB charge", cbb && cbb.present && cbb.chargeValid ? cbb.charge + "%" : "-"),
        row("Cell voltage", cbb && cbb.present && cbb.cellVoltageValid ? (cbb.cellVoltage / 1000000).toFixed(2) + " V" : "-"),
        row("Current", cbb && cbb.present && cbb.currentValid ? (cbb.current / 1000).toFixed(1) + " mA" : "-"),
        row("Temperature", cbb && cbb.present && cbb.temperatureValid ? cbb.temperature + " °C" : "-"),
        row("Cycles / health", cbb && cbb.present ? (cbb.cycleCountValid ? cbb.cycleCount : "-") + " / " + (cbb.stateOfHealthValid ? cbb.stateOfHealth + "%" : "-") : "-"),
        row("Capacity", cbb && cbb.present ? (cbb.remainingCapacityValid ? (cbb.remainingCapacity / 1000000).toFixed(2) : "-") + " / " + (cbb.fullCapacityValid ? (cbb.fullCapacity / 1000000).toFixed(2) : "-") + " Ah" : "-")
    ]
    readonly property var vehicleRows: [
        row("State", vehicle ? vehicle.stateRaw : "-"), row("Kickstand", vehicle ? wire("kickstand", vehicle.kickstand) : "-"),
        row("Handlebar lock", vehicle ? wire("handleBarLockSensor", vehicle.handleBarLockSensor) : "-"),
        row("Seatbox button", vehicle ? wire("toggle", vehicle.seatboxButton) : "-"),
        row("Blinker lamp", vehicle ? wire("blinkerState", vehicle.blinkerState) : "-"),
        row("Main power", vehicle ? wire("toggle", vehicle.mainPower) : "-"),
        row("Speed", engine && engine.hasSpeed ? engine.speed.toFixed(1) + " km/h" : "-"),
        row("RPM", engine ? engine.rpm.toFixed(0) : "-"),
        row("Odometer", engine ? (engine.odometer / 1000).toFixed(1) + " km" : "-"),
        row("Motor V / A", engine ? voltage(engine.motorVoltage) + " / " + current(engine.motorCurrent) : "-"),
        row("Motor temp", engine ? engine.temperature + " °C" : "-")
    ]
    readonly property var gpsRows: [
        row("State", gps ? wire("gpsState", gps.gpsState) : "-"), row("Fix", gps ? gps.fix : "-"),
        row("Satellites used", gps ? gps.satellitesUsed : "-"), row("Satellites visible", gps ? gps.satellitesVisible : "-"),
        row("Accuracy", gps && gps.eph > 0 ? gps.eph + " m" : "-"),
        row("Signal (SNR)", gps && gps.snr > 0 ? gps.snr.toFixed(1) + " dBHz" : "-"), row("Latitude", gps && gps.hasValidGps ? gps.latitude.toFixed(6) : "-"),
        row("Longitude", gps && gps.hasValidGps ? gps.longitude.toFixed(6) : "-"), row("Altitude", gps && gps.hasValidGps ? gps.altitude.toFixed(1) + " m" : "-"),
        row("Course", gps && gps.hasValidGps ? gps.course.toFixed(1) + "°" : "-")
    ]
    readonly property var networkRows: [
        row("Modem", net ? wire("modemState", net.modemState) : "-"), row("Connectivity", net ? net.connectivity : "-"),
        row("Status", net ? wire("connectionStatus", net.status) : "-"), row("Cloud", net && net.hasUnuCloud ? wire("connectionStatus", net.unuCloud) : "-"),
        row("IP", net ? net.ipAddress : "-"), row("Access tech", net ? net.accessTech : "-"), row("Signal", net ? net.signalQuality + "%" : "-"),
        row("Health", net ? net.modemHealth : "-"), row("IMEI", net ? net.simImei : "-"), row("ICCID", net ? net.simIccid : "-"), row("IMSI", net ? net.simImsi : "-")
    ]
    readonly property var modemRows: [
        row("Operator", modem ? modem.operatorName : "-"), row("Operator code", modem ? modem.operatorCode : "-"),
        row("Registration", modem ? modem.registration : "-"), row("Roaming", modem ? modem.isRoaming : "-"),
        row("Power", modem ? modem.powerState : "-"), row("SIM state", modem ? modem.simState : "-"), row("SIM lock", modem ? modem.simLock : "-"),
        row("PIN action", modem ? modem.pinAction : "-"), row("APN action", modem ? modem.apnAction : "-"),
        row("Reg. failure", modem ? modem.registrationFail : "-"), row("Error", modem ? modem.errorState : "-")
    ]
    readonly property var softwareRows: {
        var rows = []
        var source = info ? info.diagnosticVersionRows : []
        for (var i = 0; i < source.length; ++i) rows.push(source[i])
        rows.push(row("BMS 0", pack(bat0, bat0 ? bat0.firmwareVersion : "")))
        rows.push(row("BMS 1", pack(bat1, bat1 ? bat1.firmwareVersion : "")))
        return rows
    }
    readonly property var updateRows: [
        row("MDB status", ota ? ota.mdbStatus : "-"), row("MDB version", ota ? ota.mdbUpdateVersion : "-"),
        row("MDB download", ota ? ota.mdbDownloadProgress + "%" : "-"), row("MDB install", ota ? ota.mdbInstallProgress + "%" : "-"),
        row("MDB error", ota ? ota.mdbError : "-"), row("MDB message", ota ? ota.mdbErrorMessage : "-"),
        row("DBC status", ota ? ota.dbcStatus : "-"), row("DBC version", ota ? ota.dbcUpdateVersion : "-"),
        row("DBC download", ota ? ota.dbcDownloadProgress + "%" : "-"), row("DBC install", ota ? ota.dbcInstallProgress + "%" : "-"),
        row("DBC error", ota ? ota.dbcError : "-"), row("DBC message", ota ? ota.dbcErrorMessage : "-")
    ]
    readonly property var selectedRows: tabIndex === 1 ? powerRows : tabIndex === 3 ? vehicleRows : tabIndex === 4 ? gpsRows : tabIndex === 5 ? networkRows : tabIndex === 6 ? modemRows : tabIndex === 7 ? softwareRows : updateRows

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            Layout.fillWidth: true; Layout.preferredHeight: 66
            Text { x: 14; y: 8; text: root.serviceActive ? "Service diagnostics" : "Debug diagnostics"; color: "white"; font.pixelSize: root.fontSize; font.family: root.fontFamily }
            Text { x: 14; y: 38; text: root.tabs[root.tabIndex]; color: "white"; font.pixelSize: 20; font.bold: true; font.family: root.fontFamily }
            Text { anchors.right: parent.right; anchors.rightMargin: 14; y: 38; text: (root.tabIndex + 1) + " / " + root.tabs.length; color: root.muted; font.pixelSize: root.fontSize; font.family: root.fontFamily }
            Rectangle { x: 14; y: 33; width: parent.width - 28; height: 1; color: "#454545" }
        }
        Flickable {
            id: flick
            objectName: "debugScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; boundsBehavior: Flickable.StopAtBounds
            contentWidth: width; contentHeight: page.height
            Column {
                id: page
                x: 14; width: flick.width - 28; spacing: 0
                Loader { width: parent.width; sourceComponent: root.tabIndex === 0 ? overviewPage : root.tabIndex === 2 ? comparisonPage : rowsPage }
            }
        }
        ControlHints {
            Layout.fillWidth: true
            visible: root.vehicle && root.vehicle.parked
            isDark: true; reservedRows: 2
            leftTap: typeof translations !== "undefined" ? translations.controlNextTab : "Next tab"
            rightTap: typeof translations !== "undefined" ? translations.controlPreviousTab : "Previous tab"
            rightHold: root.canScroll ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll down") : ""
            leftHold: root.serviceActive ? "" : (typeof translations !== "undefined" ? translations.controlBack : "Back")
            leftHoldLong: root.serviceActive ? (typeof translations !== "undefined" ? translations.controlExitServiceMode : "Exit service mode") : ""
        }
    }

    component Card: Rectangle {
        color: "#171717"; border.color: "#5a5a5a"; radius: typeof themeStore !== "undefined" ? themeStore.radiusCard : 5; clip: true
    }
    component DiagnosticText: Text {
        color: "white"; font.family: root.fontFamily; font.pixelSize: root.fontSize
        wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText
    }
    component ReadingCard: Card {
        id: reading
        property string title: ""
        property string primary: "-"
        property string status: "-"
        property string detail: ""
        property color statusColor: root.muted
        property bool compact: false
        implicitHeight: compact ? 83 : 104
        Column {
            anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8; anchors.topMargin: 4
            DiagnosticText { text: reading.title; color: root.muted; width: parent.width; wrapMode: Text.NoWrap }
            DiagnosticText { text: reading.primary; font.pixelSize: 26; font.bold: true; width: parent.width; wrapMode: Text.NoWrap }
            DiagnosticText { objectName: "readingStatus"; text: reading.status; color: reading.statusColor; width: parent.width; wrapMode: Text.NoWrap }
            DiagnosticText { text: reading.detail; color: root.muted; width: parent.width; visible: !reading.compact; wrapMode: Text.NoWrap }
        }
    }
    component PackReading: Card {
        id: packReading
        property var packStore: null
        property string title: ""
        implicitHeight: 83
        Column {
            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; anchors.topMargin: 4
            Item {
                width: parent.width; height: root.fontSize + 3
                DiagnosticText { objectName: "packTitle"; anchors.left: parent.left; width: parent.width / 2; text: packReading.title; color: root.muted; wrapMode: Text.NoWrap }
                DiagnosticText { objectName: "packState"; anchors.right: parent.right; width: parent.width / 2; horizontalAlignment: Text.AlignRight; text: root.packState(packReading.packStore); color: root.muted; wrapMode: Text.NoWrap }
            }
            DiagnosticText { width: parent.width; wrapMode: Text.NoWrap; text: root.pack(packReading.packStore, packReading.packStore ? packReading.packStore.charge + "%" : ""); font.pixelSize: 26; font.bold: true }
            DiagnosticText { width: parent.width; wrapMode: Text.NoWrap; text: packReading.packStore && packReading.packStore.present ? root.voltage(packReading.packStore.voltage) + "  " + root.current(packReading.packStore.current) : "-"; color: root.muted }
        }
    }
    component SwitchTile: Card {
        id: tile
        property string title: ""
        property string stateText: "-"
        property bool active: false
        color: active ? "#173222" : "#171717"
        border.color: active ? "#4caf50" : "#555555"
        implicitHeight: 56
        Column {
            anchors.centerIn: parent
            DiagnosticText { width: tile.width - 8; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.NoWrap; text: tile.title }
            DiagnosticText { width: tile.width - 8; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.NoWrap; text: tile.stateText; color: tile.active ? root.good : "white" }
        }
    }
    Component {
        id: overviewPage
        Column {
            objectName: "debugPowerPanel"
            width: flick.width - 28; spacing: 5
            Row {
                width: parent.width; spacing: 6
                ReadingCard {
                    objectName: "auxCard"; width: (parent.width - 6) / 2
                    title: "AUX"; primary: root.aux && root.aux.voltageValid ? (root.aux.voltage / 1000).toFixed(2) + " V" : "-"
                    status: root.auxStage(); statusColor: root.aux && root.aux.chargeStatusValid && root.aux.chargeStatus !== Scooter.AuxChargeStatus.NotCharging ? root.good : root.muted; detail: "Est. " + root.auxCharge()
                }
                ReadingCard {
                    objectName: "cbbCard"; width: (parent.width - 6) / 2
                    title: "CBB"; primary: root.cbb && root.cbb.present && root.cbb.chargeValid ? root.cbb.charge + "%" : "-"
                    status: root.cbbStage(); statusColor: root.cbb && root.cbb.present && root.cbb.chargeStatus === Scooter.ChargeStatus.Charging ? root.good : root.muted; detail: root.cbb && root.cbb.present && root.cbb.cellVoltageValid ? (root.cbb.cellVoltage / 1000000).toFixed(2) + " V" : "-"
                }
            }
            Row {
                width: parent.width; spacing: 6
                PackReading { objectName: "main0Card"; width: (parent.width - 6) / 2; title: "BATTERY 0"; packStore: root.bat0 }
                PackReading { objectName: "main1Card"; width: (parent.width - 6) / 2; title: "BATTERY 1"; packStore: root.bat1 }
            }
            Row {
                width: parent.width; spacing: 4
                SwitchTile { objectName: "brakeLeftTile"; width: (parent.width - 12) / 4; title: "BRAKE L"; active: root.vehicle && root.vehicle.brakeLeft === Scooter.Toggle.On; stateText: root.vehicle ? root.wire("toggle", root.vehicle.brakeLeft).toUpperCase() : "-" }
                SwitchTile { objectName: "brakeRightTile"; width: (parent.width - 12) / 4; title: "BRAKE R"; active: root.vehicle && root.vehicle.brakeRight === Scooter.Toggle.On; stateText: root.vehicle ? root.wire("toggle", root.vehicle.brakeRight).toUpperCase() : "-" }
                SwitchTile { objectName: "blinkerTile"; width: (parent.width - 12) / 4; title: "BLINKER"; stateText: root.vehicle ? root.wire("blinkerSwitch", root.vehicle.blinkerSwitch).toUpperCase() : "-"; active: stateText !== "OFF" && stateText !== "-" }
                SwitchTile { objectName: "seatboxTile"; width: (parent.width - 12) / 4; title: "SEATBOX"; stateText: root.vehicle ? root.wire("seatboxLock", root.vehicle.seatboxLock).toUpperCase() : "-"; active: root.vehicle && root.vehicle.seatboxLock === Scooter.SeatboxLock.Open }
            }
            Row {
                width: parent.width; spacing: 6
                Card { width: (parent.width - 6) / 2; height: 35; DiagnosticText { objectName: "gpsSummary"; anchors.centerIn: parent; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.NoWrap; text: root.gpsSummary() } }
                Card { width: (parent.width - 6) / 2; height: 35; DiagnosticText { objectName: "lteSummary"; anchors.centerIn: parent; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.NoWrap; text: root.lteSummary() } }
            }
            Card { width: parent.width; height: 34; DiagnosticText { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Odometer"; width: parent.width / 2 - 12; wrapMode: Text.NoWrap; color: root.muted } DiagnosticText { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: parent.width / 2 - 12; horizontalAlignment: Text.AlignRight; wrapMode: Text.NoWrap; text: root.engine ? (root.engine.odometer / 1000).toFixed(1) + " km" : "-" } }
        }
    }
    component ComparisonLine: Item {
        id: comparisonLine
        property string label: ""
        property string leftValue: "-"
        property string rightValue: "-"
        property bool large: false
        property bool stateBadge: false
        width: parent.width
        height: Math.max(28, labelText.implicitHeight, leftText.implicitHeight, rightText.implicitHeight)
        Rectangle { visible: comparisonLine.stateBadge; x: parent.width * 0.23 - 4; y: 0; width: leftText.implicitWidth + 8; height: leftText.implicitHeight + 2; radius: 3; color: comparisonLine.leftValue === "active" ? "#173b24" : "#303030" }
        Rectangle { visible: comparisonLine.stateBadge; x: parent.width * 0.615 - 4; y: 0; width: rightText.implicitWidth + 8; height: rightText.implicitHeight + 2; radius: 3; color: comparisonLine.rightValue === "active" ? "#173b24" : "#303030" }
        DiagnosticText { id: labelText; x: 0; width: parent.width * 0.23; text: comparisonLine.label; color: root.muted }
        DiagnosticText { id: leftText; x: parent.width * 0.23; width: parent.width * 0.385 - 8; text: comparisonLine.leftValue; font.pixelSize: comparisonLine.large ? 25 : root.fontSize }
        DiagnosticText { id: rightText; x: parent.width * 0.615; width: parent.width * 0.385; text: comparisonLine.rightValue; font.pixelSize: comparisonLine.large ? 25 : root.fontSize }
    }
    Component {
        id: comparisonPage
        Card {
            width: flick.width - 28; height: comparisonColumn.height + 20
            Column {
                id: comparisonColumn; x: 10; y: 10; width: parent.width - 20; spacing: 0
                ComparisonLine { label: ""; leftValue: "Battery 0"; rightValue: "Battery 1" }
                ComparisonLine { label: "State"; stateBadge: true; leftValue: root.packState(root.bat0); rightValue: root.packState(root.bat1) }
                ComparisonLine { label: "Charge"; large: true; leftValue: root.pack(root.bat0, root.bat0 ? root.bat0.charge + "%" : ""); rightValue: root.pack(root.bat1, root.bat1 ? root.bat1.charge + "%" : "") }
                Item {
                    width: parent.width; height: 10
                    Rectangle { x: parent.width * 0.23; width: parent.width * 0.35; height: 4; color: "#444"; Rectangle { width: parent.width * (root.bat0 && root.bat0.present ? Math.max(0, Math.min(100, root.bat0.charge)) / 100 : 0); height: parent.height; color: root.good } }
                    Rectangle { x: parent.width * 0.615; width: parent.width * 0.35; height: 4; color: "#444"; Rectangle { width: parent.width * (root.bat1 && root.bat1.present ? Math.max(0, Math.min(100, root.bat1.charge)) / 100 : 0); height: parent.height; color: root.good } }
                }
                Repeater { model: root.comparison; delegate: ComparisonLine { label: modelData.label; leftValue: modelData.left; rightValue: modelData.right } }
            }
        }
    }
    component DataRow: Item {
        id: dataRow
        property string label: ""
        property string value: "-"
        width: parent.width
        height: Math.max(29, labelText.implicitHeight + 6, valueText.implicitHeight + 6)
        DiagnosticText { id: labelText; x: 0; y: 3; width: parent.width * 0.37; text: dataRow.label; color: root.muted }
        DiagnosticText { id: valueText; x: parent.width * 0.37; y: 3; width: parent.width * 0.63; text: dataRow.value }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#303030" }
    }
    Component {
        id: rowsPage
        Card {
            width: flick.width - 28; height: rowsColumn.height + 12
            Column {
                id: rowsColumn; x: 10; y: 6; width: parent.width - 20
                Repeater { model: root.selectedRows; delegate: DataRow { label: modelData.label; value: modelData.value } }
            }
        }
    }
}
