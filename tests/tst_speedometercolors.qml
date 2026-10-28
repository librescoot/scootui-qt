import QtQuick
import QtTest
import "../qml/widgets/speedometer"
import "../qml/widgets/power"

// The fill ramp has configurable origin, normal, warning and overspeed stops.
// These checks keep each configured colour attached to its named speed.
TestCase {
    name: "SpeedometerColors"
    when: windowShown

    QtObject {
        id: engineStore
        property real speed: 0
        property real rawSpeed: 0
        property bool hasRawSpeed: false
        property real motorCurrent: 0
        property real motorVoltage: 54000
        property bool regenAvailable: true
        property string regenReason: "none"
        property int faultCode: 0
        property bool dataStale: false
    }

    QtObject {
        id: settingsStore
        property bool showRawSpeed: false
        property int speedometerMaxSpeed: 60
        property int speedometerNormalSpeed: 30
        property int speedometerWarnSpeed: 55
        property int speedometerOverspeed: 60
        property string speedometerOriginColor: "#90CAF9"
        property string speedometerBaseColor: "#2196F3"
        property string speedometerWarnColor: "#9C27B0"
        property string speedometerOverspeedColor: "#E91E63"
        property int speedometerRegenMinPower: 50
        property string speedometerRegenColor: "#4DFF0000"
        property string speedometerErrorColor: "#F44336"
        property string powerRegenColor: "#43A047"
        property string powerDischargeColor: "#1E88E5"
        property string powerBoostColor: "#FB8C00"
        property int powerDisplayMode: 0
        property bool dualBattery: false
        property string showRoadName: "always"
        property string showSpeedLimit: "always"
    }

    QtObject {
        id: notificationService
        property bool telemetryTrustLost: false
        property bool startupSpeedPlaceholder: false
    }

    QtObject {
        id: speedLimitStore
        property string speedLimit: ""
        property string roadName: ""
        property string roadRefs: ""
        property string roadType: ""
        property string roadSignStyle: ""
    }

    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontDisplay: 48
        property int fontTitle: 16
        property int fontCaption: 12
        property int fontMicro: 9
        property int fontBody: 16
        property color textHint: "#99ffffff"
        property int radiusBar: 8
        property int radiusCard: 12
    }

    SpeedometerDisplay {
        id: speedometer
        width: 300
        height: 240
    }

    QtObject {
        id: translations
        property string powerRegen: "Regen"
        property string powerDischarge: "Discharge"
    }

    PowerDisplay {
        id: powerDisplay
        width: 200
        height: 56
    }

    function init() {
        settingsStore.speedometerMaxSpeed = 60
        settingsStore.speedometerNormalSpeed = 30
        settingsStore.speedometerWarnSpeed = 55
        settingsStore.speedometerOverspeed = 60
        settingsStore.speedometerOriginColor = "#90CAF9"
        settingsStore.speedometerBaseColor = "#2196F3"
        settingsStore.speedometerWarnColor = "#9C27B0"
        settingsStore.speedometerOverspeedColor = "#E91E63"
        settingsStore.speedometerRegenMinPower = 50
        settingsStore.speedometerRegenColor = "#4DFF0000"
        settingsStore.speedometerErrorColor = "#F44336"
        settingsStore.powerRegenColor = "#43A047"
        settingsStore.powerDischargeColor = "#1E88E5"
        settingsStore.powerBoostColor = "#FB8C00"
        themeStore.isDark = true
        engineStore.speed = 0
        engineStore.rawSpeed = 0
        engineStore.hasRawSpeed = false
        engineStore.faultCode = 0
        engineStore.dataStale = false
        settingsStore.showRawSpeed = false
        notificationService.telemetryTrustLost = false
        notificationService.startupSpeedPlaceholder = false
        engineStore.motorVoltage = 54000
        engineStore.motorCurrent = 0
        speedometer.errorPulse = 0
        speedometer.animatedSpeed = 0
        speedometer.overspeedPulse = 1
    }

    function test_startupZeroPreservesFaultAndMovingMasks() {
        const digits = findChild(speedometer, "speedometerDigits")
        notificationService.telemetryTrustLost = true
        notificationService.startupSpeedPlaceholder = true
        compare(digits.text, "0")
        speedometer.animatedSpeed = 42
        compare(digits.text, "0")
        engineStore.faultCode = 20
        compare(digits.text, "—")
        engineStore.faultCode = 0
        engineStore.speed = 12
        compare(digits.text, "—")
        engineStore.speed = 0
        engineStore.rawSpeed = 12
        engineStore.hasRawSpeed = true
        settingsStore.showRawSpeed = true
        compare(digits.text, "—")
        settingsStore.showRawSpeed = false
        compare(digits.text, "—")
        engineStore.rawSpeed = 0
        settingsStore.showRawSpeed = true
        engineStore.speed = 12
        compare(digits.text, "—")
        engineStore.speed = 0
        compare(digits.text, "0")
        settingsStore.showRawSpeed = false
        notificationService.startupSpeedPlaceholder = false
        compare(digits.text, "—")
        notificationService.telemetryTrustLost = false
        speedometer.animatedSpeed = 0
        compare(digits.text, "0")
        engineStore.speed = 12
        engineStore.dataStale = true
        speedometer.animatedSpeed = 12
        compare(digits.text, "12")
    }

    function test_regenTintRequiresMinimumChargingPower() {
        engineStore.motorVoltage = 50000
        engineStore.motorCurrent = -999
        fuzzyCompare(speedometer.regenPowerW, 49.95, 0.001)
        verify(!speedometer.isRegenerating)

        engineStore.motorCurrent = -1000
        compare(speedometer.regenPowerW, 50)
        verify(speedometer.isRegenerating)
        tryCompare(speedometer, "regenTransition", 1, 1500)

        settingsStore.speedometerRegenMinPower = 75
        verify(!speedometer.isRegenerating)
        tryCompare(speedometer, "regenTransition", 0, 1500)
        compare(speedometer.trackColor, speedometer.trackBaseColor)

        engineStore.motorVoltage = 75000
        verify(speedometer.isRegenerating)
        engineStore.motorCurrent = 1000
        verify(!speedometer.isRegenerating)
        engineStore.motorCurrent = 0
        verify(!speedometer.isRegenerating)
    }

    function test_zeroThresholdRequiresNegativeCurrent() {
        settingsStore.speedometerRegenMinPower = 0
        engineStore.motorCurrent = 0
        verify(!speedometer.isRegenerating)
        engineStore.motorCurrent = 1
        verify(!speedometer.isRegenerating)
        engineStore.motorCurrent = -1
        verify(speedometer.isRegenerating)
    }

    function test_regenTintUsesConfiguredColor() {
        settingsStore.speedometerRegenColor = "#8000FF00"
        compare(speedometer.regenTintColor.toString(), "#8000ff00")
        speedometer.regenTransition = 1
        tryCompare(speedometer, "regenTransition", 1, 1500)
        compare(speedometer.trackColor.toString(), "#8000ff00")
    }

    function test_errorPulseUsesConfiguredColorAndAlpha() {
        settingsStore.speedometerErrorColor = "#8000FF00"
        compare(speedometer.errorColor.toString(), "#8000ff00")
        speedometer.errorPulse = 0
        fuzzyCompare(speedometer.errorPulseColor.a, (128 / 255) * 0.15, 0.005)
        speedometer.errorPulse = 1
        compare(speedometer.errorPulseColor.r, 0)
        compare(speedometer.errorPulseColor.g, 1)
        compare(speedometer.errorPulseColor.b, 0)
        fuzzyCompare(speedometer.errorPulseColor.a, (128 / 255) * 0.5, 0.005)
    }

    function test_powerColorsApplyAtRuntime() {
        compare(powerDisplay.regenColor.toString(), "#43a047")
        compare(powerDisplay.dischargeColor.toString(), "#1e88e5")
        compare(powerDisplay.boostColor.toString(), "#fb8c00")
        settingsStore.powerRegenColor = "#80123456"
        settingsStore.powerDischargeColor = "#abcdef"
        settingsStore.powerBoostColor = "#fedcba"
        compare(powerDisplay.regenColor.toString(), "#80123456")
        compare(powerDisplay.dischargeColor.toString(), "#abcdef")
        compare(powerDisplay.boostColor.toString(), "#fedcba")
        compare(powerDisplay.reasonColdColor.toString(), "#40c4ff")
        compare(powerDisplay.reasonHotColor.toString(), "#ff6e40")
        compare(powerDisplay.reasonFullColor.toString(), "#69f0ae")
    }

    function test_shippedColorsReachNamedStops() {
        speedometer.animatedSpeed = 0
        compare(speedometer.speedFillColor.toString(), "#90caf9")

        speedometer.animatedSpeed = settingsStore.speedometerNormalSpeed
        compare(speedometer.speedFillColor.toString(), "#2196f3")

        speedometer.animatedSpeed = settingsStore.speedometerWarnSpeed
        compare(speedometer.speedFillColor.toString(), "#9c27b0")

        speedometer.animatedSpeed = settingsStore.speedometerOverspeed
        compare(speedometer.speedFillColor.toString(), "#e91e63")
    }

    function test_configuredColorsReachTheArc() {
        settingsStore.speedometerOriginColor = "#010203"
        settingsStore.speedometerBaseColor = "#123456"
        settingsStore.speedometerWarnColor = "#FFDE21"
        settingsStore.speedometerOverspeedColor = "#00FF00"

        speedometer.animatedSpeed = 0
        compare(speedometer.speedFillColor.toString(), "#010203")
        speedometer.animatedSpeed = settingsStore.speedometerNormalSpeed
        compare(speedometer.speedFillColor.toString(), "#123456")
        speedometer.animatedSpeed = settingsStore.speedometerWarnSpeed
        compare(speedometer.speedFillColor.toString(), "#ffde21")
        speedometer.animatedSpeed = settingsStore.speedometerOverspeed
        compare(speedometer.speedFillColor.toString(), "#00ff00")
    }

    function test_hyperspaceStartsPastConfiguredMaximum() {
        engineStore.speed = settingsStore.speedometerMaxSpeed
        verify(!speedometer.hyperspaceActive)

        engineStore.speed += 1
        verify(speedometer.hyperspaceActive)

        settingsStore.speedometerMaxSpeed = 75
        engineStore.speed = 75
        verify(!speedometer.hyperspaceActive)

        engineStore.speed = 76
        verify(speedometer.hyperspaceActive)
    }

    // The overspeed pulse spans the two configured colours.
    function test_overspeedPulseUsesConfiguredColors() {
        settingsStore.speedometerWarnColor = "#FFDE21"
        settingsStore.speedometerOverspeedColor = "#00FF00"
        speedometer.animatedSpeed = settingsStore.speedometerOverspeed + 1

        speedometer.overspeedPulse = 0
        compare(speedometer.speedFillColor.toString(), "#ffde21")
        speedometer.overspeedPulse = 1
        compare(speedometer.speedFillColor.toString(), "#00ff00")
    }

    function test_interpolatesBetweenStops() {
        settingsStore.speedometerOriginColor = "#000000"
        settingsStore.speedometerBaseColor = "#202020"
        settingsStore.speedometerWarnColor = "#404040"
        settingsStore.speedometerOverspeedColor = "#606060"

        speedometer.animatedSpeed = settingsStore.speedometerNormalSpeed / 2
        compare(speedometer.speedFillColor.toString(), "#101010")

        speedometer.animatedSpeed = (settingsStore.speedometerNormalSpeed
                                     + settingsStore.speedometerWarnSpeed) / 2
        compare(speedometer.speedFillColor.toString(), "#303030")
    }

    function test_outOfOrderSpeedsCollapseSafely() {
        settingsStore.speedometerNormalSpeed = 80
        settingsStore.speedometerWarnSpeed = 70
        settingsStore.speedometerOverspeed = 60

        compare(speedometer.normalSpeed, 60)
        compare(speedometer.warningSpeed, 60)
    }
}
