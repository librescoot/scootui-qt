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
        property color textColor: "white"
        property color textSecondary: "gray"
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
}
