import QtQuick
import QtTest
import ScootUI 1.0
import ScootUITest 1.0
import "../qml/screens"

TestCase {
    name: "DebugDiagnostics"
    when: windowShown
    visible: true
    width: 480
    height: 480

    QtObject {
        id: auxBatteryStore
        property bool voltageValid: false
        property int voltage: 12500
        property bool chargeValid: false
        property int charge: 100
        property bool chargeStatusValid: false
        property int chargeStatus: Scooter.AuxChargeStatus.FloatCharge
    }
    QtObject {
        id: cbBatteryStore
        property bool present: false
        property bool chargeValid: false
        property int charge: 100
        property int cellVoltage: 3900000
        property int current: -12500
        property bool currentValid: true
        property bool cellVoltageValid: true
        property bool temperatureValid: true
        property bool cycleCountValid: true
        property bool stateOfHealthValid: true
        property bool remainingCapacityValid: true
        property bool fullCapacityValid: true
        property int temperature: 23
        property int cycleCount: 12
        property int stateOfHealth: 98
        property int remainingCapacity: 900000
        property int fullCapacity: 1000000
        property int chargeStatus: Scooter.ChargeStatus.Unknown
    }
    QtObject {
        id: battery0Store
        property bool present: true
        property int charge: 82
        property int voltage: 52400
        property int current: -1200
        property int temperature0: 22
        property int temperature1: 23
        property int temperature2: 24
        property int temperature3: 23
        property int cycleCount: 142
        property int stateOfHealth: 96
        property string firmwareVersion: "v4.2.1"
        property string serialNumber: "EXAMPLE-PACK-0"
        property int batteryState: Scooter.BatteryState.Active
    }
    QtObject {
        id: battery1Store
        property bool present: false
        property int charge: 0
        property int voltage: 0
        property int current: 0
        property int temperature0: 21
        property int temperature1: 22
        property int temperature2: 23
        property int temperature3: 22
        property int cycleCount: 87
        property int stateOfHealth: 98
        property string firmwareVersion: "v4.2.1"
        property string serialNumber: "EXAMPLE-PACK-1"
        property int batteryState: Scooter.BatteryState.Unknown
    }
    QtObject {
        id: vehicleStore
        property bool parked: true
        property string stateRaw: "parked"
        property int brakeLeft: Scooter.Toggle.Off
        property int brakeRight: Scooter.Toggle.Off
        property int seatboxLock: Scooter.SeatboxLock.Closed
        property int seatboxButton: Scooter.Toggle.Off
        property int kickstand: Scooter.Kickstand.Down
        property int mainPower: Scooter.Toggle.On
        property int handleBarLockSensor: Scooter.HandleBarLockSensor.Unlocked
        property int blinkerState: Scooter.BlinkerState.Off
        property int blinkerSwitch: Scooter.BlinkerSwitch.Off
    }
    EnumStrings { id: enumStrings }
    QtObject {
        id: settingsStore
        property string serviceActive: "true"
    }
    QtObject {
        id: settingsService
        property int exits: 0
        property int modeChanges: 0
        function disableServiceMode() { exits++ }
        function updateMode(mode) { modeChanges++ }
    }
    QtObject {
        id: systemInfoService
        property int loads: 0
        property var diagnosticVersionRows: [
            { label: "MDB", value: "v1.4.0-12-g8f3c1a2" },
            { label: "MDB Built", value: "2026-11-05 12:34:56" },
            { label: "DBC", value: "v1.4.0-12-g8f3c1a2" },
            { label: "DBC Built", value: "2026-11-05 12:45:00" },
            { label: "nRF", value: "1.14.0" },
            { label: "ECU", value: "v3.4.5" }
        ]
        function loadVersions() { loads++ }
    }
    QtObject {
        id: inputHandler
        signal leftTap()
        signal rightTap()
        signal rightHold()
        signal leftHold()
        signal leftBrakeHold()
    }
    QtObject {
        id: engineStore
        property bool hasSpeed: true
        property real speed: 0
        property real rpm: 0
        property real odometer: 12345000
        property real motorVoltage: 51200
        property real motorCurrent: 0
        property real temperature: 24
    }
    QtObject {
        id: gpsStore
        property int gpsState: Scooter.GpsState.FixEstablished
        property string fix: "3D"
        property int satellitesUsed: 9
        property int satellitesVisible: 12
        property real eph: 4
        property real snr: 0
        property bool hasValidGps: true
        property real latitude: 52.5
        property real longitude: 13.4
        property real altitude: 40
        property real speed: 0
        property real course: 0
    }
    QtObject {
        id: internetStore
        property int modemState: Scooter.ModemState.Connected
        property string connectivity: "online"
        property int status: 0
        property bool hasUnuCloud: false
        property int unuCloud: 0
        property string ipAddress: "192.0.2.1"
        property string accessTech: "LTE"
        property int signalQuality: 76
        property string modemHealth: "ok"
        property string simImei: "EXAMPLE-IMEI"
        property string simIccid: "EXAMPLE-ICCID"
        property string simImsi: "EXAMPLE-IMSI"
    }
    QtObject {
        id: themeStore
        property bool isDark: true
        property real fontBody: 18
        property real fontMicro: 10
        property real radiusCard: 8
    }
    QtObject { id: menuStore; property bool isOpen: false }
    Component { id: screenComponent; DebugScreen { width: 480; height: 480 } }

    function init() {
        battery1Store.present = false
        gpsStore.gpsState = Scooter.GpsState.FixEstablished
        gpsStore.snr = 0
        internetStore.modemState = Scooter.ModemState.Connected
        internetStore.signalQuality = 76
        auxBatteryStore.chargeStatus = Scooter.AuxChargeStatus.FloatCharge
        battery0Store.voltage = 52400
        battery0Store.serialNumber = "EXAMPLE-PACK-0"
        vehicleStore.blinkerSwitch = Scooter.BlinkerSwitch.Off
        menuStore.isOpen = false
        settingsStore.serviceActive = "true"
        auxBatteryStore.voltageValid = false
        auxBatteryStore.chargeValid = false
        auxBatteryStore.chargeStatusValid = false
        auxBatteryStore.charge = 100
        cbBatteryStore.present = false
        cbBatteryStore.chargeValid = false
        vehicleStore.brakeLeft = Scooter.Toggle.Off
        vehicleStore.brakeRight = Scooter.Toggle.Off
        vehicleStore.seatboxLock = Scooter.SeatboxLock.Closed
        settingsService.exits = 0
        settingsService.modeChanges = 0
        systemInfoService.loads = 0
    }

    function test_overviewLiveAndMissingData() {
        const view = createTemporaryObject(screenComponent, this)
        verify(view !== null)
        waitForRendering(view)
        const panel = findChild(view, "debugPowerPanel")
        const scroll = findChild(view, "debugScroll")
        verify(panel.height <= scroll.height)
        const aux = findChild(view, "auxCard")
        const cbb = findChild(view, "cbbCard")
        const main = findChild(view, "main0Card")
        verify(aux.mapToItem(panel, 0, 0).x < cbb.mapToItem(panel, 0, 0).x)
        verify(cbb.mapToItem(panel, 0, 0).y < main.mapToItem(panel, 0, 0).y)
        compare(aux.primary, "-")
        compare(cbb.primary, "-")
        auxBatteryStore.voltage = 0
        auxBatteryStore.voltageValid = true
        compare(aux.primary, "0.00 V")
        auxBatteryStore.chargeValid = true
        auxBatteryStore.charge = 255
        compare(view.auxCharge(), "Error")
        cbBatteryStore.present = true
        cbBatteryStore.chargeValid = true
        cbBatteryStore.charge = 0
        compare(cbb.primary, "0%")
        cbBatteryStore.currentValid = false
        compare(view.powerRows[6].value, "-")
        cbBatteryStore.current = 0
        cbBatteryStore.currentValid = true
        compare(view.powerRows[6].value, "0.0 mA")
        engineStore.hasSpeed = false
        compare(view.vehicleRows[6].value, "-")
        engineStore.hasSpeed = true
        compare(view.comparison[0].right, "-")
        battery0Store.voltage = 50000
        compare(view.comparison[0].left, "50.0 V")
        compare(view.comparison[5].left, "v4.2.1")
        compare(view.comparison[6].left, "EXAMPLE-PACK-0")
        vehicleStore.blinkerSwitch = Scooter.BlinkerSwitch.Left
        compare(findChild(view, "blinkerTile").stateText, "LEFT")
    }
    function test_overviewProductionEnumsAndTextBounds() {
        const view = createTemporaryObject(screenComponent, this)
        const panel = findChild(view, "debugPowerPanel")
        const gpsText = findChild(panel, "gpsSummary")
        const lteText = findChild(panel, "lteSummary")
        const aux = findChild(panel, "auxCard")
        const missing = findChild(panel, "main1Card")
        function checkTextBounds(item) {
            if (item.visible && item.text !== undefined && item.paintedWidth !== undefined) {
                verify(item.font.pixelSize >= 17, "small text: " + item.text)
                verify(item.paintedWidth <= item.width + 0.5,
                       "horizontal overflow: " + item.text + " " + item.paintedWidth + "/" + item.width)
                verify(item.paintedHeight <= item.height + 0.5, "vertical overflow: " + item.text)
                const pos = item.mapToItem(item.parent, 0, 0)
                verify(pos.x >= -0.5 && pos.x + item.width <= item.parent.width + 0.5,
                       "text bounds outside parent: " + item.text)
            }
            for (let i = 0; i < item.children.length; ++i)
                checkTextBounds(item.children[i])
        }
        compare(enumStrings.gpsState(Scooter.GpsState.FixEstablished), "fix-established")
        compare(enumStrings.gpsState(Scooter.GpsState.Searching), "searching")
        compare(enumStrings.modemState(Scooter.ModemState.Disconnected), "disconnected")
        compare(gpsText.text, "GPS Fix · 9 sats")
        compare(lteText.text, "LTE 76%")
        compare(findChild(missing, "packState").text, "Not present")
        auxBatteryStore.chargeStatusValid = true
        auxBatteryStore.chargeStatus = Scooter.AuxChargeStatus.AbsorptionCharge
        compare(findChild(aux, "readingStatus").text, "absorption-charge")
        waitForRendering(view)
        checkTextBounds(panel)
        gpsStore.gpsState = Scooter.GpsState.Searching
        internetStore.modemState = Scooter.ModemState.Disconnected
        compare(gpsText.text, "GPS searching")
        compare(lteText.text, "LTE disconnected")
        waitForRendering(view)
        checkTextBounds(panel)
        if (captureDirectory.length > 0)
            grabImage(view).save(captureDirectory + "/overview-missing-searching.png")
        gpsStore.gpsState = Scooter.GpsState.Off
        internetStore.modemState = Scooter.ModemState.Off
        compare(gpsText.text, "GPS off")
        compare(lteText.text, "LTE off")
        gpsStore.gpsState = Scooter.GpsState.Error
        compare(gpsText.text, "GPS error")
        waitForRendering(view)
        checkTextBounds(panel)
    }
    function test_gpsSignalAndPlainText() {
        const view = createTemporaryObject(screenComponent, this)
        compare(view.gpsRows[5].label, "Signal (SNR)")
        compare(view.gpsRows[5].value, "-")
        gpsStore.snr = 38.25
        compare(view.gpsRows[5].value, "38.3 dBHz")
        battery0Store.serialNumber = "<b>EXAMPLE</b>"
        view.tabIndex = 2
        waitForRendering(view)
        function findText(item, value) {
            if (item.text === value && item.textFormat !== undefined) return item
            for (let i = 0; i < item.children.length; ++i) {
                const found = findText(item.children[i], value)
                if (found) return found
            }
            return null
        }
        const serial = findText(view, "<b>EXAMPLE</b>")
        verify(serial !== null)
        compare(serial.textFormat, Text.PlainText)
        verify(serial.paintedWidth > 0)
    }
    function test_controlsAndOverflow() {
        const view = createTemporaryObject(screenComponent, this)
        verify(view !== null)
        compare(systemInfoService.loads, 1)
        inputHandler.rightTap()
        compare(view.tabIndex, 8)
        inputHandler.leftTap()
        compare(view.tabIndex, 0)
        menuStore.isOpen = true
        inputHandler.leftTap()
        compare(view.tabIndex, 0)
        menuStore.isOpen = false
        view.visible = false
        inputHandler.leftTap()
        compare(view.tabIndex, 0)
        view.visible = true
        inputHandler.leftHold()
        compare(settingsService.modeChanges, 0)
        inputHandler.leftBrakeHold()
        compare(settingsService.exits, 1)
        settingsStore.serviceActive = "false"
        inputHandler.leftHold()
        compare(settingsService.modeChanges, 1)
        settingsStore.serviceActive = "true"
        battery0Store.serialNumber = "EXAMPLE-VERY-LONG-SERIAL-".repeat(20)
        view.tabIndex = 2
        wait(50)
        verify(view.canScroll)
        inputHandler.rightHold()
        const scroll = findChild(view, "debugScroll")
        verify(scroll.contentY > 0)
        for (let i = 0; i < 100 && scroll.contentY + scroll.height < scroll.contentHeight - 1; ++i)
            inputHandler.rightHold()
        verify(scroll.contentY + scroll.height >= scroll.contentHeight - 1)
        inputHandler.rightHold()
        compare(scroll.contentY, 0)
        battery0Store.serialNumber = "EXAMPLE-PACK-0"
    }
    function test_allTabsFitAndDiagnosticFonts() {
        const view = createTemporaryObject(screenComponent, this)
        const scroll = findChild(view, "debugScroll")
        function checkFonts(item) {
            if (item.visible && item.text !== undefined && item.font !== undefined)
                verify(item.font.pixelSize >= 17, "small diagnostic text: " + item.text)
            for (let i = 0; i < item.children.length; ++i)
                checkFonts(item.children[i])
        }
        for (let i = 0; i < 9; ++i) {
            view.tabIndex = i
            wait(10)
            verify(scroll.contentHeight <= scroll.height, "ordinary values overflow tab " + i)
            checkFonts(scroll)
        }
    }
    function test_capture() {
        const view = createTemporaryObject(screenComponent, this)
        gpsStore.snr = 38
        auxBatteryStore.voltage = 12600
        auxBatteryStore.voltageValid = true
        auxBatteryStore.chargeValid = true
        auxBatteryStore.charge = 100
        auxBatteryStore.chargeStatusValid = true
        cbBatteryStore.present = true
        cbBatteryStore.chargeValid = true
        cbBatteryStore.charge = 84
        cbBatteryStore.chargeStatus = Scooter.ChargeStatus.Charging
        battery1Store.present = true
        battery1Store.charge = 64
        battery1Store.voltage = 50100
        battery1Store.batteryState = Scooter.BatteryState.Idle
        waitForRendering(view)
        if (captureDirectory.length > 0) {
            grabImage(view).save(captureDirectory + "/overview.png")
            view.tabIndex = 2
            waitForRendering(view)
            grabImage(view).save(captureDirectory + "/batteries.png")
            view.tabIndex = 4
            waitForRendering(view)
            grabImage(view).save(captureDirectory + "/gps.png")
            view.tabIndex = 7
            waitForRendering(view)
            grabImage(view).save(captureDirectory + "/software.png")
        }
    }
}
