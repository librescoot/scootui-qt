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
    }

    QtObject {
        id: settingsStore
        property bool showRawSpeed: false
    }

    QtObject {
        id: notificationService
        property bool telemetryTrustLost: false
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
    }

    SpeedCenterWidget {
        id: speedCenter
        width: 120
        height: 80
    }

    function test_connectionLossInvalidatesSpeed() {
        compare(speedCenter.displayedSpeed, "42")
        notificationService.telemetryTrustLost = true
        compare(speedCenter.displayedSpeed, "—")
        notificationService.telemetryTrustLost = false
        compare(speedCenter.displayedSpeed, "42")
    }

    function test_tripDurationFormattingHandlesZeroAndLongValues() {
        compare(tripBar.formatDuration(0), "00:00")
        compare(tripBar.formatDuration(3599), "59:59")
        compare(tripBar.formatDuration(3661), "1:01")
        compare(tripBar.formatDuration(2147483647), "596523:14")
    }
}
