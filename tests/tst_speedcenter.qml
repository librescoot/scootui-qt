import QtQuick
import QtTest
import "../qml/widgets/status_bars"

TestCase {
    name: "SpeedCenterWidget"
    when: windowShown

    QtObject {
        id: engineStore
        property real speed: 42
        property real rawSpeed: 42
        property bool hasRawSpeed: false
        property int faultCode: 0
        property real odometer: 0
        property bool dataStale: false
    }

    QtObject {
        id: settingsStore
        property bool showRawSpeed: false
    }

    QtObject {
        id: notificationService
        property bool telemetryTrustLost: false
        property bool startupSpeedPlaceholder: false
    }

    QtObject {
        id: themeStore
        property int fontXL: 40
        property int fontBody: 14
        property int fontCaption: 10
        property color textColor: "white"
        property color textSecondary: "gray"
        property color backgroundColor: "black"
        property color borderColor: "gray"
        property bool isDark: true
    }

    QtObject {
        id: tripStore
        property real distance: 0
        property int duration: 0
        property real averageSpeed: 0
    }

    QtObject {
        id: translations
        property string statusBarDuration: "Duration"
        property string statusBarAvgSpeed: "Avg"
        property string statusBarTrip: "Trip"
        property string statusBarTotal: "Total"
    }

    UnifiedBottomStatusBar {
        id: tripBar
        width: 480
        centerContentHeight: barCenter.implicitHeight
        SpeedCenterWidget { id: barCenter }
    }

    SpeedCenterWidget {
        id: speedCenter
        width: 120
        height: 80
    }

    function test_bottomBarHeightIndependentOfAnchoredPositions() {
        const left = findChild(tripBar, "bottomBarLeft")
        const right = findChild(tripBar, "bottomBarRight")
        const center = findChild(tripBar, "bottomBarCenter")
        for (const dark of [true, false]) {
            themeStore.isDark = dark
            for (const size of [14, 24, 18]) {
                themeStore.fontBody = size
                compare(tripBar.implicitHeight,
                        Math.max(left.implicitHeight, right.implicitHeight, barCenter.implicitHeight) + 8)
                compare(center.height, barCenter.implicitHeight)
                compare(left.y + left.height, tripBar.height - 2)
                compare(right.y + right.height, tripBar.height - 2)
            }
        }
        themeStore.fontBody = 14
        themeStore.isDark = true
    }

    function test_startupZeroPreservesFaultAndMovingMasks() {
        engineStore.speed = 0
        notificationService.telemetryTrustLost = true
        notificationService.startupSpeedPlaceholder = true
        compare(speedCenter.displayedSpeed, "0")

        engineStore.faultCode = 20
        compare(speedCenter.displayedSpeed, "—")
        engineStore.faultCode = 0
        engineStore.speed = 12
        compare(speedCenter.displayedSpeed, "—")
        engineStore.speed = 0
        engineStore.hasRawSpeed = true
        settingsStore.showRawSpeed = true
        compare(speedCenter.displayedSpeed, "—")
        settingsStore.showRawSpeed = false
        compare(speedCenter.displayedSpeed, "—")
        engineStore.rawSpeed = 0
        settingsStore.showRawSpeed = true
        engineStore.speed = 12
        compare(speedCenter.displayedSpeed, "—")
        engineStore.speed = 0
        compare(speedCenter.displayedSpeed, "0")
        settingsStore.showRawSpeed = false
        engineStore.hasRawSpeed = false
        engineStore.rawSpeed = 42

        notificationService.startupSpeedPlaceholder = false
        compare(speedCenter.displayedSpeed, "—")
        notificationService.telemetryTrustLost = false
        compare(speedCenter.displayedSpeed, "0")
        engineStore.speed = 42
    }

    function test_connectionLossInvalidatesSpeed() {
        compare(speedCenter.displayedSpeed, "42")
        notificationService.telemetryTrustLost = true
        compare(speedCenter.displayedSpeed, "—")
        notificationService.telemetryTrustLost = false
        compare(speedCenter.displayedSpeed, "42")
    }

    function test_valueChangeTimeoutDoesNotInvalidateSteadySpeed() {
        compare(speedCenter.displayedSpeed, "42")
        engineStore.dataStale = true
        compare(speedCenter.displayedSpeed, "42")

        engineStore.speed = 0
        compare(speedCenter.displayedSpeed, "0")

        engineStore.dataStale = false
        engineStore.speed = 42
        compare(speedCenter.displayedSpeed, "42")
    }

    function test_tripDurationFormattingHandlesZeroAndLongValues() {
        compare(tripBar.formatDuration(0), "00:00")
        compare(tripBar.formatDuration(3599), "59:59")
        compare(tripBar.formatDuration(3661), "1:01")
        compare(tripBar.formatDuration(2147483647), "596523:14")
    }
}
