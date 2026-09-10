import QtQuick
import QtTest
import ScootUITest 1.0
import "../qml/screens"
import "../qml/widgets/components"

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
    property var harness: null
    Component { id: nativeComponent; NativeAttentionHarness {} }
    SignalSpy { id: nativeEventCues; target: harness ? harness.notifications : null; signalName: "eventPresented" }
    SignalSpy { id: legacyToastCues; target: harness ? harness.toasts : null; signalName: "toastAdded" }

    readonly property var translations: harness ? harness.translations : ({
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
    readonly property string longTitle: "Multiple critical issues require attention before continuing your journey safely"
    readonly property string longBody: "Stop safely and check the vehicle before continuing; additional diagnostic details are available"
    readonly property var allCounts: ({error: 12, warning: 23, success: 34, info: 45, debug: 56})
    readonly property var longNavigation: ({kind: "nav", status: 2, maneuverType: 17, distance: 80,
        instruction: "Take the third exit onto the very long road name towards the central railway station and continue straight ahead",
        nextStreet: "Central station", nextType: 5, showNextPreview: true,
        remainingDuration: 1200, distanceToDestination: 6500, eta: "18:10"})
    readonly property var states: [
        {name: "idle", main: {}, companion: {}},
        {name: "navigation", main: navigation, companion: {}},
        {name: "navigation-success", main: navigation, companion: {kind: "success", priority: 3, title: "Download complete"}, queuedCounts: {info: 2}},
        {name: "navigation-info", main: navigation, companion: {kind: "info", priority: 4, title: "Trip updated"}},
        {name: "warning", main: {kind: "warning", priority: 2, title: "Battery warning"}, companion: navigation},
        {name: "critical", main: {kind: "critical", priority: 0, title: "Stop safely", body: "Check vehicle"}, companion: navigation},
        {name: "navigation-long-success-counts", main: longNavigation,
         companion: {kind: "success", priority: 3, title: longTitle, body: longBody}, queuedCounts: {success: 12, info: 23, debug: 34}},
        {name: "navigation-long-info-counts", main: longNavigation,
         companion: {kind: "info", priority: 4, title: longTitle, body: longBody}, queuedCounts: {info: 23, debug: 34}},
        {name: "warning-long-counts", main: {kind: "warning", priority: 2, title: longTitle, body: longBody},
         companion: longNavigation, queuedCounts: {warning: 23, success: 34, info: 45, debug: 56}},
        {name: "error-long-counts", main: {kind: "error", priority: 0, title: longTitle, body: longBody},
         companion: longNavigation, queuedCounts: allCounts},
        {name: "critical-long-counts", main: {kind: "critical", priority: 0, title: longTitle, body: longBody},
         companion: longNavigation, queuedCounts: allCounts},
        {name: "loading-success-counts", main: {kind: "nav", status: 3, maneuverType: 17, distance: 80},
         companion: {kind: "success", priority: 3, title: longTitle, body: longBody}, queuedCounts: {info: 23, debug: 34}},
        {name: "navigation-long-only", main: longNavigation, companion: {}},
        {name: "error-long-only", main: {kind: "error", priority: 0, title: longTitle, body: longBody},
         companion: {}, queuedCounts: allCounts}
    ]
    function init() {
        notificationService.presentation = ({})
        notificationService.telemetryTrustLost = false
        engineStore.speed = 0
        themeStore.isDark = true
        themeStore.backgroundColor = "black"
    }
    function point(item, screen) { return item.mapToItem(screen, item.width / 2, item.height / 2) }
    function matchesInk(pixel, ink) {
        return pixel.a > 0.9 && Math.abs(pixel.r - ink.r) < 0.06
                && Math.abs(pixel.g - ink.g) < 0.06 && Math.abs(pixel.b - ink.b) < 0.06
    }
    function maneuverPainted(screen, image, widget) {
        const box = findChild(widget, "maneuverIconBox")
        let icon
        let ink = widget.isDark ? Qt.rgba(1, 1, 1, 1) : Qt.rgba(33/255, 33/255, 33/255, 1)
        if (box.isRoundabout) {
            icon = findChild(widget, "mainRoundaboutIcon")
            if (!icon) return false
            if (!icon.hasMap && !widget.isDark) ink = Qt.rgba(0.2, 0.2, 0.2, 1)
        } else if (box.isKeepFork) {
            icon = findChild(widget, "maneuverForkIcon")
            if (icon.status !== Image.Ready) return false
        } else {
            icon = findChild(widget, "maneuverGlyph")
        }
        if (!icon || !icon.visible) return false
        const origin = icon.mapToItem(screen, 0, 0)
        // Restrict the screen pixels to the actual icon, entirely left of the
        // dial. Neither the speedometer arc nor the subdued ring is route ink.
        const dial = findChild(screen, "speedometerDial")
        if (dial) verify(origin.x + icon.width <= dial.mapToItem(screen, 0, 0).x)
        let painted = 0
        for (let y = Math.ceil(origin.y); y < Math.floor(origin.y + icon.height); ++y) {
            for (let x = Math.ceil(origin.x); x < Math.floor(origin.x + icon.width); ++x) {
                if (matchesInk(image.pixel(x, y), ink)) ++painted
            }
        }
        return painted >= 8
    }
    function capture(screen, name) {
        let image
        tryVerify(function() {
            image = grabImage(screen)
            for (const objectName of ["attentionTurnByTurn", "attentionCompanion"]) {
                const widget = findChild(screen, objectName)
                if (widget && widget.visible && (widget.navigating || widget.loading)
                        && !maneuverPainted(screen, image, widget)) return false
            }
            return image.width > 0 && image.height > 0
        }, 2000, name + ": maneuver ink must be rendered before capture")
        if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
            image.save(captureDirectory + "/" + name + ".png")
    }
    TextMetrics { id: speedGlyphMetrics }
    function paintedGlyphBounds(digits, screen) {
        speedGlyphMetrics.font = digits.font
        speedGlyphMetrics.text = digits.text
        const glyphs = speedGlyphMetrics.tightBoundingRect
        verify(glyphs.width > 0 && glyphs.height > 0, "Speed readout must paint glyphs")
        const baseline = digits.mapToItem(screen, 0, digits.baselineOffset)
        return {left: Math.floor(baseline.x + glyphs.x), top: Math.floor(baseline.y + glyphs.y),
                right: Math.ceil(baseline.x + glyphs.x + glyphs.width),
                bottom: Math.ceil(baseline.y + glyphs.y + glyphs.height)}
    }
    function verifyUnobscuredGlyphs(screen, overlay, bounds, reference, stateName) {
        if (overlay.visible)
            verify(overlay.mapToItem(screen, 0, overlay.height).y <= bounds.top,
                   stateName + ": overlay must end above painted speed glyphs at " + bounds.top)
        const rendered = grabImage(screen)
        for (let y = bounds.top; y <= bounds.bottom; ++y) {
            for (let x = bounds.left; x <= bounds.right; ++x) {
                if (!Qt.colorEqual(rendered.pixel(x, y), reference.pixel(x, y)))
                    fail(stateName + ": obscured speed pixel " + x + "," + y)
            }
        }
    }
    function test_clusterAlignment_data() {
        const cases = []
        for (const dark of [true, false]) {
            for (const speed of [0, 8, 18, 28, 50, -1])
                cases.push({tag: (speed < 0 ? "dash" : "speed-" + speed) + (dark ? "-dark" : "-light"), speed: speed, dark: dark})
        }
        return cases
    }
    function test_clusterAlignment(data) {
        themeStore.isDark = data.dark
        themeStore.backgroundColor = data.dark ? "black" : "white"
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
        waitForRendering(screen)
        const bounds = paintedGlyphBounds(digits, screen)
        const reference = grabImage(screen)
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
            verifyUnobscuredGlyphs(screen, overlay, bounds, reference, state.name)
            capture(screen, "cluster-" + (data.speed < 0 ? "dash" : data.speed) + "-" + state.name + (data.dark ? "" : "-light"))
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
    function cleanup() {
        notificationService.presentation = ({})
        harness = null
    }

    function useNative() {
        harness = createTemporaryObject(nativeComponent, this)
        verify(harness !== null)
        compare(harness.vehicle.state, 2) // ReadyToDrive
        notificationService.presentation = Qt.binding(function() { return harness ? harness.notifications.presentation : ({}) })
        return createTemporaryObject(clusterComponent, this)
    }

    function test_maneuverInkRejectsBackground_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }
    function test_maneuverInkRejectsBackground(data) {
        themeStore.isDark = data.dark
        themeStore.backgroundColor = data.dark ? "black" : "white"
        const screen = createTemporaryObject(clusterComponent, this)
        waitForRendering(screen)
        const background = grabImage(screen)
        for (const type of [2, 3, 17, 18]) {
            notificationService.presentation = {main: Object.assign({}, navigation, {maneuverType: type})}
            waitForRendering(screen)
            const widget = findChild(screen, "attentionTurnByTurn")
            verify(!maneuverPainted(screen, background, widget), "Background alone is not a maneuver")
            capture(screen, "maneuver-ink-" + type + "-" + data.tag)
        }
    }

    function test_specializedManeuverCaptureTransitions_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }
    function test_specializedManeuverCaptureTransitions(data) {
        themeStore.isDark = data.dark
        themeStore.backgroundColor = data.dark ? "black" : "white"
        const screen = createTemporaryObject(clusterComponent, this)
        for (const mode of ["full", "secondary", "companion"]) {
            for (const type of [2, 3, 17, 18]) {
                for (const status of [2, 1, 2, 3, 2]) {
                    const nav = Object.assign({}, navigation, {maneuverType: type, status: status})
                    notificationService.presentation = mode === "companion"
                        ? {main: {kind: "warning", priority: 2, title: "Warning"}, companion: nav}
                        : {main: nav, companion: mode === "secondary"
                           ? {kind: "success", priority: 3, title: "Download complete"} : {}}
                    waitForRendering(screen)
                    const widget = findChild(screen, mode === "companion" ? "attentionCompanion" : "attentionTurnByTurn")
                    const box = findChild(widget, "maneuverIconBox")
                    compare(box.isRoundabout, status === 2 && type >= 17)
                    compare(box.isKeepFork, status === 2 && type < 17)
                    compare(findChild(widget, "navigationLoadingIndicator").running, status !== 2)
                    capture(screen, "maneuver-transition-" + mode + "-" + type + "-" + status + "-" + data.tag)
                }
            }
        }
    }

    function test_nativeToastSuccessSecondary_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }
    function test_nativeToastSuccessSecondary(data) {
        themeStore.isDark = data.dark
        themeStore.backgroundColor = data.dark ? "black" : "white"
        const screen = useNative()
        nativeEventCues.clear()
        legacyToastCues.clear()
        verify(harness.loadFixture("route1"))
        harness.approach()
        harness.toasts.showInfo("Trip updated")
        harness.toasts.showSuccess("Download complete")
        waitForRendering(screen)
        compare(harness.notifications.presentation.main.kind, "nav")
        const secondary = findChild(screen, "attentionCompanionNotification")
        verify(secondary.visible)
        compare(secondary.entry.kind, "success")
        compare(secondary.entry.priority, 3)
        compare(findChild(secondary, "notificationTitle").text, "Download complete")
        const icon = findChild(secondary, "notificationIcon")
        compare(icon.text, MaterialIcon.iconCheckCircleOutline)
        verify(icon.color.g > icon.color.r && icon.color.g > icon.color.b)
        compare(findChild(screen, "queuedCount_info").count, 1)
        const rendered = grabImage(screen)
        const origin = icon.mapToItem(screen, 0, 0)
        let greenPixels = 0
        for (let y = Math.ceil(origin.y); y < Math.floor(origin.y + icon.height); ++y)
            for (let x = Math.ceil(origin.x); x < Math.floor(origin.x + icon.width); ++x) {
                const pixel = rendered.pixel(x, y)
                if (pixel.a > 0.9 && pixel.g > pixel.r + 0.1 && pixel.g > pixel.b + 0.1) ++greenPixels
            }
        verify(greenPixels >= 8, "Adapted success must paint a green icon")
        capture(screen, "harness-toast-success-" + data.tag)
        compare(nativeEventCues.count, 2)
        compare(nativeEventCues.signalArguments[1][0], "success")
        harness.toasts.showSuccess("Download complete")
        compare(nativeEventCues.count, 2)
        compare(legacyToastCues.count, 0)
        harness.toasts.showWarning("Warning")
        waitForRendering(screen)
        compare(findChild(screen, "queuedCount_success").count, 1)
        capture(screen, "harness-toast-success-queued-" + data.tag)
        harness.advance(2999)
        compare(harness.notifications.presentation.queuedCounts.success, 1)
        harness.advance(1)
        compare(harness.notifications.presentation.main.kind, "nav")
        compare(harness.notifications.presentation.companion, {})
    }

    function test_nativeNavigationSecondaryVisibility() {
        const screen = useNative()
        verify(harness.loadFixture("route1"))
        harness.approach()
        compare(harness.navigation.status, 2)
        verify(harness.receive(JSON.stringify({id: "a", title: "Trip updated", severity: "info"})))
        verify(harness.receive(JSON.stringify({id: "b", title: "Phone connected", severity: "info"})))
        waitForRendering(screen)
        const tbt = findChild(screen, "attentionTurnByTurn")
        verify(tbt.visible)
        let secondary = findChild(screen, "attentionCompanionNotification")
        verify(secondary.visible)
        compare(findChild(secondary, "notificationTitle").text, "Trip updated")
        compare(findChild(tbt, "queuedCount_info").count, 1)
        capture(screen, "harness-navigation-info")
        harness.advance(2000)
        verify(harness.receive(JSON.stringify({id: "a", title: "Trip updated again", severity: "info"})))
        harness.advance(2999)
        compare(findChild(secondary, "notificationTitle").text, "Trip updated again")
        harness.advance(1)
        compare(findChild(secondary, "notificationTitle").text, "Phone connected")
        verify(harness.receive(JSON.stringify({id: "done", title: "Download complete", severity: "success"})))
        waitForRendering(screen)
        compare(findChild(secondary, "notificationTitle").text, "Download complete")
        compare(findChild(tbt, "queuedCount_info").count, 2)
        capture(screen, "harness-navigation-success")
        verify(harness.receive(JSON.stringify({id: "error", title: "Stop safely", severity: "error"})))
        waitForRendering(screen)
        verify(findChild(screen, "attentionMainCard").visible)
        verify(findChild(screen, "attentionCompanion").visible)
        compare(findChild(findChild(screen, "attentionMainCard"), "queuedCount_success").count, 1)
        capture(screen, "harness-error-navigation-counts")
        verify(harness.receive(JSON.stringify({id: "error", action: "dismiss"})))
        verify(harness.receive(JSON.stringify({id: "done", action: "dismiss"})))
        harness.advance(5000)
        waitForRendering(screen)
        secondary = findChild(screen, "attentionCompanionNotification")
        compare(findChild(secondary, "notificationTitle").text, "Trip updated again")
        harness.advance(2000)
        waitForRendering(screen)
        verify(findChild(screen, "attentionTurnByTurn").visible)
        compare(harness.notifications.presentation.companion, {})
        compare(harness.notifications.presentation.queuedCounts, {})
        compare(findChild(screen, "speedometerDial").width, 300)
        compare(findChild(screen, "speedometerDial").height, 240)
    }

    function test_nativeCalculatingRoute() {
        const screen = useNative()
        verify(harness.loadFixture("route1"))
        harness.calculate()
        compare(harness.navigation.status, 1)
        verify(harness.receive(JSON.stringify({id: "loading-info", title: "Trip updated", severity: "info"})))
        waitForRendering(screen)
        const tbt = findChild(screen, "attentionTurnByTurn")
        verify(tbt.visible)
        verify(findChild(tbt, "navigationLoadingIndicator").running)
        compare(findChild(tbt, "maneuverInstruction").text, harness.translations.navCalculating)
        verify(!findChild(tbt, "maneuverDistance").visible)
        verify(!findChild(tbt, "maneuverTripSummary").visible)
        verify(findChild(screen, "attentionCompanionNotification").visible)
        compare(harness.arrivalEvents, 0)
        wait(200) // Capture the loading indicator after its entrance animation.
        compare(harness.navigation.status, 1)
        waitForRendering(screen)
        capture(screen, "harness-calculating-route")
        harness.navigation.clearNavigation()
    }

    function test_nativeArrival_data() {
        return [{tag: "right", fixture: "route1", type: 21, text: "Your destination is on your right"},
                {tag: "left", fixture: "route3", type: 22, text: "Your destination is on your left"},
                {tag: "unsided", fixture: "route6", type: 20, text: "Your destination is here"}]
    }

    function test_nativeArrival(data) {
        const screen = useNative()
        verify(harness.loadFixture(data.fixture))
        harness.approach()
        compare(harness.navigation.status, 2)
        harness.arrive()
        waitForRendering(screen)
        compare(harness.navigation.status, 4)
        compare(harness.navigation.currentManeuverType, data.type)
        let tbt = findChild(screen, "attentionTurnByTurn")
        compare(findChild(tbt, "maneuverIconBox").mType, data.type)
        compare(findChild(tbt, "maneuverInstruction").text, data.text)
        verify(!findChild(tbt, "maneuverDistance").visible)
        let secondary = findChild(screen, "attentionCompanionNotification")
        verify(secondary.visible)
        compare(secondary.entry.kind, "success")
        compare(findChild(secondary, "notificationTitle").text, "You have arrived")
        capture(screen, "harness-arrival-" + data.tag)
        harness.translations.setLanguage("de")
        waitForRendering(screen)
        const german = data.type === 21 ? "Dein Ziel ist auf der rechten Seite"
                     : data.type === 22 ? "Dein Ziel ist auf der linken Seite" : "Dein Ziel ist hier"
        compare(findChild(tbt, "maneuverInstruction").text, german)
        harness.notifications.publishCondition("test-warning", "test", "Warning", "", 2, "warning")
        waitForRendering(screen)
        const compact = findChild(screen, "attentionCompanion")
        compare(findChild(compact, "maneuverIconBox").mType, data.type)
        compare(findChild(compact, "maneuverInstruction").text, german)
        harness.notifications.resolveCondition("test-warning")
        harness.translations.setLanguage("en")
        waitForRendering(screen)
        tbt = findChild(screen, "attentionTurnByTurn")
        secondary = findChild(screen, "attentionCompanionNotification")
        harness.arrive()
        harness.reconnect()
        wait(150) // Let NavigationService's store-refresh debounce run.
        verify(harness.recalculate())
        harness.depart()
        compare(harness.arrivalEvents, 1)
        compare(harness.navigation.status, 4)
        harness.advance(9999)
        verify(secondary.visible)
        harness.advance(1)
        compare(harness.notifications.presentation.companion, {})
        compare(harness.notifications.presentation.main.status, 4)
        compare(findChild(tbt, "maneuverInstruction").text, data.text)
        harness.navigation.clearNavigation()
        compare(harness.notifications.presentation.main, {})
        verify(harness.loadFixture(data.fixture))
        harness.arrive()
        compare(harness.arrivalEvents, 2)
        harness.depart()
        harness.calculate()
        compare(harness.notifications.presentation.companion, {})
        compare(harness.navigation.status, 1)
        verify(harness.loadFixture(data.fixture))
        compare(harness.notifications.presentation.companion, {})
        compare(harness.navigation.status, 2)
    }

}
