import QtQuick
import QtTest
import "../qml/widgets/speedometer"

// The fill ramp is driven by the three advanced colour settings. This checks
// that the configured hex reaches the arc and that no setting leaves the
// shipped ramp in place.
TestCase {
    name: "SpeedometerColors"
    when: windowShown

    QtObject {
        id: engineStore
        property real speed: 0
        property real rawSpeed: 0
        property bool hasRawSpeed: false
        property real motorCurrent: 0
        property int faultCode: 0
        property bool dataStale: false
    }

    QtObject {
        id: settingsStore
        property bool showRawSpeed: false
        property int speedometerMaxSpeed: 60
        property int speedometerWarnSpeed: 55
        property int speedometerOverspeed: 60
        property string speedometerBaseColor: "#2196F3"
        property string speedometerWarnColor: "#9C27B0"
        property string speedometerOverspeedColor: "#E91E63"
        property string showRoadName: "always"
        property string showSpeedLimit: "always"
    }

    QtObject {
        id: notificationService
        property bool telemetryTrustLost: false
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
        property int radiusBar: 8
        property int radiusCard: 12
    }

    SpeedometerDisplay {
        id: speedometer
        width: 300
        height: 240
    }

    function init() {
        settingsStore.speedometerBaseColor = "#2196F3"
        settingsStore.speedometerWarnColor = "#9C27B0"
        settingsStore.speedometerOverspeedColor = "#E91E63"
        speedometer.animatedSpeed = 0
    }

    function test_shippedRampReachesBaseAtWarnSpeed() {
        speedometer.animatedSpeed = settingsStore.speedometerWarnSpeed
        compare(speedometer.speedFillColor.toString(), "#2196f3")
    }

    function test_shippedRampReachesWarnColorAtOverspeed() {
        speedometer.animatedSpeed = settingsStore.speedometerOverspeed
        compare(speedometer.speedFillColor.toString(), "#9c27b0")
    }

    function test_configuredColorsReachTheArc() {
        settingsStore.speedometerBaseColor = "#123456"
        settingsStore.speedometerWarnColor = "#FFDE21"
        settingsStore.speedometerOverspeedColor = "#00FF00"

        speedometer.animatedSpeed = settingsStore.speedometerWarnSpeed
        compare(speedometer.speedFillColor.toString(), "#123456")

        speedometer.animatedSpeed = settingsStore.speedometerOverspeed
        compare(speedometer.speedFillColor.toString(), "#ffde21")
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

    // The bottom of the ramp follows the base hue instead of staying blue.
    function test_lowEndFollowsBaseHue() {
        settingsStore.speedometerBaseColor = "#123456"
        var base = speedometer.baseSpeedColor
        var low = speedometer.lowSpeedColor
        verify(low.r > base.r)
        verify(low.g > base.g)
        verify(low.b > base.b)
    }

    // The shipped base still lands on the Material Blue 200 the arc used before
    // the colours became configurable, give or take rounding.
    function test_lowEndPreservesShippedBlue() {
        var low = speedometer.lowSpeedColor
        verify(near(Math.round(low.r * 255), 0x90))
        verify(near(Math.round(low.g * 255), 0xCA))
        verify(near(Math.round(low.b * 255), 0xF9))
    }

    function near(actual, expected) {
        return Math.abs(actual - expected) <= 1
    }
}
