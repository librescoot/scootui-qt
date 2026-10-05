import QtQuick
import QtTest
import ScootUITest 1.0
import "../qml/screens"

TestCase {
    name: "MaintenanceNotifications"
    when: windowShown
    visible: true
    width: 480
    height: 480

    QtObject {
        id: themeStore
        property bool isDark: true
        property int fontBody: 18
        property int fontTitle: 20
        property int radiusModal: 16
    }
    QtObject {
        id: vehicleStore
        property int state: 0
        property string stateRaw: "unknown"
    }
    QtObject {
        id: dashboardStore
        function setBacklightEnabled(enabled) {}
    }
    QtObject {
        id: otaStore
        property bool isActive: false
        property string dbcStatus: "idle"
        property int dbcDownloadProgress: 40
        property int dbcInstallProgress: 0
        property string dbcUpdateVersion: "testing-local"
    }
    property var harness: null
    readonly property var notificationService: harness ? harness.notifications : null
    readonly property var translations: harness ? harness.translations : null
    Component { id: nativeComponent; NativeAttentionHarness {} }
    Component { id: screenComponent; MaintenanceScreen { width: 480; height: 480 } }

    function init() {
        themeStore.isDark = true
        otaStore.isActive = false
        otaStore.dbcStatus = "idle"
        vehicleStore.stateRaw = "unknown"
        harness = createTemporaryObject(nativeComponent, this)
        verify(harness !== null)
    }
    function cleanup() { harness = null }
    function screen(connection) {
        const result = createTemporaryObject(screenComponent, this, {showConnectionInfo: connection})
        verify(result !== null)
        waitForRendering(result)
        return result
    }
    function card(screen) { return findChild(screen, "maintenanceAttention") }
    function origin(item, screen) { return item.mapToItem(screen, 0, 0) }
    function capture(screen, name) {
        waitForRendering(screen)
        const image = grabImage(screen)
        compare(image.width, 480)
        compare(image.height, 480)
        if (typeof captureDirectory !== "undefined" && captureDirectory.length > 0)
            image.save(captureDirectory + "/maintenance-" + name + ".png")
    }
    function connectionFits(screen) {
        const attention = card(screen)
        const indicator = findChild(screen, "maintenanceState")
        let bottom = attention.visible ? attention.height : 0
        for (const name of ["connectionTitle", "connectionDetails", "connectionOverrideHint"]) {
            const text = findChild(screen, name)
            verify(text !== null)
            const point = origin(text, screen)
            verify(point.y >= bottom, name + " must not overlap the notification or preceding text")
            verify(point.x >= 0 && point.x + text.width <= screen.width)
            bottom = point.y + text.height
            verify(bottom <= origin(indicator, screen).y - 8,
                   name + " bottom=" + bottom + " must fit above state y=" + origin(indicator, screen).y)
            verify(!text.truncated)
        }
    }

    function test_connectionErrorAndWarning_data() {
        return [{tag: "dark", dark: true}, {tag: "light", dark: false}]
    }
    function test_connectionErrorAndWarning(data) {
        themeStore.isDark = data.dark
        const view = screen(true)
        verify(!card(view).visible)
        const originalY = origin(findChild(view, "connectionTitle"), view).y
        harness.toasts.showPermanentWarning("USB connection interrupted", "usb-disconnect")
        harness.toasts.showPermanentError("System connection lost", "redis-disconnect")
        waitForRendering(view)
        verify(card(view).visible)
        compare(card(view).entry.kind, "error")
        compare(findChild(card(view), "notificationTitle").text, "System connection lost")
        compare(card(view).height, 96)
        compare(card(view).color.a, 1)
        compare(findChild(card(view), "queuedCount_warning").count, 1)
        connectionFits(view)
        capture(view, "connection-error-" + data.tag)
        harness.toasts.dismiss("redis-disconnect")
        waitForRendering(view)
        compare(card(view).entry.kind, "warning")
        compare(findChild(card(view), "notificationTitle").text, "USB connection interrupted")
        connectionFits(view)
        harness.toasts.dismiss("usb-disconnect")
        waitForRendering(view)
        verify(!card(view).visible)
        compare(origin(findChild(view, "connectionTitle"), view).y, originalY)
    }

    function test_longErrorScrollsWithoutCoveringInstructions() {
        const view = screen(true)
        verify(harness.receive(JSON.stringify({id: "long", title: "Long error details. ".repeat(6),
            body: "Stop safely and check the vehicle. ".repeat(15).slice(0, 512), severity: "error", ttl_ms: 60000})))
        waitForRendering(view)
        const attention = card(view)
        verify(attention.visible)
        compare(attention.height, 96)
        verify(attention.overflowDistance > 0)
        tryCompare(attention, "scrolling", true)
        compare(findChild(attention, "notificationBody").text.length, 512)
        connectionFits(view)
        capture(view, "connection-long-error")
    }

    function test_navigationUsesNotificationCompanion() {
        const view = screen(true)
        verify(harness.loadFixture("route1"))
        harness.approach()
        harness.notifications.publishCondition("info", "test", "Trip updated", "", 4, "info")
        waitForRendering(view)
        compare(harness.notifications.presentation.main.kind, "nav")
        verify(card(view).visible)
        compare(card(view).entry.kind, "info")
        compare(findChild(card(view), "notificationTitle").text, "Trip updated")
        harness.toasts.showPermanentWarning("USB connection interrupted", "usb-disconnect")
        waitForRendering(view)
        compare(card(view).entry.kind, "warning")
        compare(findChild(card(view), "queuedCount_info").count, 1)
        connectionFits(view)
        capture(view, "connection-with-retained-navigation")
        harness.toasts.dismiss("usb-disconnect")
        harness.notifications.resolveCondition("info")
        waitForRendering(view)
        verify(!card(view).visible)
        harness.navigation.clearNavigation()
    }

    function test_errorRotationAndExpiry() {
        const view = screen(true)
        harness.toasts.showPermanentError("System connection lost", "connection")
        harness.toasts.showPermanentError("Battery fault", "battery")
        waitForRendering(view)
        compare(findChild(card(view), "notificationTitle").text, "System connection lost")
        compare(findChild(card(view), "queuedCount_error").count, 1)
        harness.advance(5000)
        waitForRendering(view)
        compare(findChild(card(view), "notificationTitle").text, "Battery fault")
        connectionFits(view)
        harness.toasts.dismiss("connection")
        harness.toasts.dismiss("battery")
        verify(harness.receive(JSON.stringify({id: "short", title: "Brief error", severity: "error", ttl_ms: 1000})))
        waitForRendering(view)
        verify(card(view).visible)
        harness.advance(999)
        verify(card(view).visible)
        harness.advance(1)
        waitForRendering(view)
        verify(!card(view).visible)
        connectionFits(view)
    }

    function test_spinnerAndUpdateProgressReserveNotificationSpace() {
        const view = screen(false)
        harness.toasts.showPermanentError("Update failed", "update")
        waitForRendering(view)
        const attention = card(view)
        const loading = findChild(view, "maintenanceLoading")
        verify(attention.visible)
        verify(loading.visible)
        verify(origin(loading, view).y >= attention.height)
        capture(view, "spinner-error")
        otaStore.isActive = true
        otaStore.dbcStatus = "downloading"
        waitForRendering(view)
        const text = findChild(view, "otaStatusText")
        verify(text.visible)
        verify(origin(text, view).y >= attention.height)
        verify(origin(text, view).y + text.height < origin(findChild(view, "maintenanceState"), view).y)
        capture(view, "update-progress-error")
    }
}
