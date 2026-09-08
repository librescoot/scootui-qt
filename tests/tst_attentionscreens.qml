import QtQuick
import QtTest
import "../qml/screens"

TestCase {
    name: "AttentionScreenGeometry"
    when: windowShown
    visible: true
    width: 480
    height: 480

    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontDisplay: 96
        property int fontHero: 64
        property int fontXL: 48
        property int fontTitle: 20
        property int fontBody: 18
        property int fontCaption: 14
        property int fontMicro: 10
        property int radiusBar: 2
        property int radiusCard: 8
        property int radiusModal: 16
        property color backgroundColor: "black"
        property color borderColor: "#333333"
        property color textColor: "white"
        property color textSecondary: "gray"
        property color textHint: "gray"
        property color statusNeutral: "gray"
        property color statusSuccess: "green"
        property color statusWarning: "orange"
    }
    QtObject {
        id: engineStore
        property real speed: 0
        property real rawSpeed: 0
        property bool hasRawSpeed: false
        property int faultCode: 0
        property var faults: []
        property real motorCurrent: 0
        property real motorVoltage: 48
        property real odometer: 0
        property bool regenAvailable: true
        property string regenReason: ""
    }
    QtObject {
        id: vehicleStore
        property int blinkerState: 3
        property real blinkOpacity: 1
        property bool isUnableToDrive: false
        property bool mainPower: true
        property int seatboxLock: 1
        property int state: 2
    }
    QtObject {
        id: notificationService
        property var presentation: ({})
        property bool telemetryTrustLost: false
        property string surface: ""
    }
    QtObject {
        id: mapService
        property bool isReady: true
        property real vehicleOffsetY: 80
        property real mapBearing: 0
        property real rawMapBearing: 0
        property real mapZoom: 16
        property real mapTilt: 60
        property real mapLatitude: 52.52
        property real mapLongitude: 13.405
        property string styleUrl: ""
        property string routeGeoJson: ""
        property var mapThemeLayers: []
        property bool debugZoomEnabled: false
        signal vehiclePositionChanged()
    }
    QtObject {
        id: gpsStore
        property int gpsState: 2
        property bool hasRecentFix: true
        property bool hasTimestamp: true
        property real latitude: 52.52
        property real longitude: 13.405
        property string fix: "3d"
        property int satellitesUsed: 12
        property int satellitesVisible: 15
        property real eph: 5
        property real snr: 30
        property real hdop: 1
        property real pdop: 1
        property string mode: "gps"
        property real lastTtffSeconds: 5
        property string lastTtffMode: "warm"
        signal sampleChanged()
    }

    QtObject {
        id: odometerMilestoneService
        signal milestoneCelebrate(int intensity, string tag)
    }
    readonly property var translations: ({
        language: "en", dateDayMonth: "%1 %2", monthAbbrev: function(month) { return "Jun" },
        powerRegen: "Regen", powerDischarge: "Power", statusBarDuration: "Duration",
        statusBarAvgSpeed: "Avg", statusBarTrip: "Trip", statusBarOdometer: "ODO", statusBarTotal: "Total", navUnavailable: "Navigation unavailable",
        mapWaitingForGps: "Waiting for GPS fix", navSetDestination: "Set a destination",
        navThen: "Then", navCalculating: "Calculating route", navRecalculating: "Recalculating route",
        navArrived: "Arrived"
    })

    Component { id: clusterComponent; ClusterScreen { width: 480; height: 480 } }
    Component { id: mapComponent; MapScreen { width: 480; height: 480 } }

    readonly property var navigation: ({kind: "nav", status: 2, maneuverType: 5, distance: 80,
                                      instruction: "Turn right onto Main Street", street: "Main Street",
                                      compactInstruction: "Turn right",
                                      remainingDuration: 1200, distanceToDestination: 6500, eta: "18:10"})
    readonly property var states: [
        {name: "idle", main: {}, companion: {}},
        {name: "navigation", main: navigation, companion: {}},
        {name: "warning", main: {kind: "warning", priority: 2, title: "Battery warning"}, companion: navigation},
        {name: "critical", main: {kind: "critical", priority: 0, title: "Stop safely", body: "Check vehicle"}, companion: navigation}
    ]
    function init() {
        notificationService.presentation = ({})
        notificationService.telemetryTrustLost = false
        engineStore.speed = 0
    }
    function point(item, screen) { return item.mapToItem(screen, item.width / 2, item.height / 2) }
    function capture(screen, name) {
        const image = grabImage(screen)
        verify(image.width > 0 && image.height > 0)
        if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
            image.save(captureDirectory + "/" + name + ".png")
    }
    function test_clusterAlignment_data() {
        return [0, 8, 18, 28, 50, -1].map(function(speed) { return {tag: speed < 0 ? "dash" : "speed-" + speed, speed: speed} })
    }
    function test_clusterAlignment(data) {
        const screen = createTemporaryObject(clusterComponent, this)
        verify(screen !== null)
        const speedometer = findChild(screen, "clusterSpeedometer")
        const dial = findChild(screen, "speedometerDial")
        const digits = findChild(screen, "speedometerDigits")
        const unit = findChild(screen, "speedometerUnit")
        const power = findChild(screen, "clusterPowerAndTelltales")
        const blinkers = findChild(screen, "clusterBlinkers")
        const overlay = findChild(screen, "clusterAttention")
        engineStore.speed = Math.max(0, data.speed)
        speedometer.animatedSpeed = Math.max(0, data.speed)
        notificationService.telemetryTrustLost = data.speed < 0
        for (const state of states) {
            notificationService.presentation = state
            waitForRendering(screen)
            const bodyHeight = 480 - 40 - screen.bottomBarHeight
            compare(speedometer.width, 480)
            compare(speedometer.height, bodyHeight)
            compare(speedometer.mapToItem(screen, 0, 0).y, 40)
            compare(dial.width, 300)
            compare(dial.height, 240)
            compare(dial.scale, 1)
            compare(point(dial, screen).x, 240)
            fuzzyCompare(point(dial, screen).y, 40 + bodyHeight / 2 - 20, 0.5)
            compare(digits.text, data.speed < 0 ? "—" : data.speed.toString())
            compare(digits.font.pixelSize, 96)
            fuzzyCompare(point(digits, screen).x, 240, 0.5)
            compare(point(digits, screen).y, 40 + bodyHeight / 2)
            // Established alignment: numeral box centre is 10 px above the arc's centre,
            // not independently centred inside a notification-shrunken instrument.
            const arcCentre = dial.mapToItem(screen, speedometer.centerX, speedometer.centerY)
            fuzzyCompare(arcCentre.y - point(digits, screen).y, 10, 0.5)
            compare(unit.mapToItem(screen, 0, 0).y, digits.mapToItem(screen, 0, digits.height).y - 12)
            compare(power.mapToItem(screen, 0, power.height).y, 480 - screen.bottomBarHeight - 8)
            verify(blinkers.visible && blinkers.showLeft && blinkers.showRight)
            compare(findChild(blinkers, "leftBlinkerImage").status, Image.Ready)
            compare(findChild(blinkers, "rightBlinkerImage").status, Image.Ready)
            verify(blinkers.y >= overlay.y + overlay.height)
            verify(blinkers.y + blinkers.height <= 480 - screen.bottomBarHeight)
            verify(blinkers.z > overlay.z)
            capture(screen, "cluster-" + (data.speed < 0 ? "dash" : data.speed) + "-" + state.name)
        }
    }
    function test_mapViewportAndMarker() {
        const screen = createTemporaryObject(mapComponent, this)
        verify(screen !== null)
        const viewport = findChild(screen, "mapViewport")
        const marker = findChild(screen, "mapVehicleMarker")
        const blinkers = findChild(screen, "mapBlinkers")
        const overlay = findChild(screen, "mapAttention")
        for (const offset of [0, 80, 120]) {
            mapService.vehicleOffsetY = offset
            for (const state of states) {
                notificationService.presentation = state
                waitForRendering(screen)
                compare(viewport.width, 480)
                compare(viewport.height, 480 - 40 - screen.bottomBarHeight)
                compare(viewport.mapToItem(screen, 0, 0).y, 40)
                compare(point(marker, viewport).x, viewport.width / 2)
                compare(point(marker, viewport).y, viewport.height / 2 + offset)
                verify(marker.visible)
                verify(blinkers.visible && blinkers.showLeft && blinkers.showRight)
                compare(findChild(blinkers, "leftBlinkerImage").status, Image.Ready)
                compare(findChild(blinkers, "rightBlinkerImage").status, Image.Ready)
                verify(blinkers.y >= overlay.y + overlay.height)
                verify(blinkers.z > overlay.z)
                compare(mapService.vehicleOffsetY, offset)
                if (offset === 80) capture(screen, "map-" + state.name)
            }
        }
    }
}
