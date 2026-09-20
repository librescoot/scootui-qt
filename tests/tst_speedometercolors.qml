import QtQuick
import QtTest
import "../qml/widgets/speedometer"

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
        settingsStore.speedometerMaxSpeed = 60
        settingsStore.speedometerNormalSpeed = 30
        settingsStore.speedometerWarnSpeed = 55
        settingsStore.speedometerOverspeed = 60
        settingsStore.speedometerOriginColor = "#90CAF9"
        settingsStore.speedometerBaseColor = "#2196F3"
        settingsStore.speedometerWarnColor = "#9C27B0"
        settingsStore.speedometerOverspeedColor = "#E91E63"
        engineStore.speed = 0
        speedometer.animatedSpeed = 0
        speedometer.overspeedPulse = 1
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
