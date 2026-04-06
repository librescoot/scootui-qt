import QtQuick
import QtQuick.Effects
import ScootUI

Item {
    id: batteryIcon
    width: 24; height: 24

    required property int charge
    required property int batteryState
    required property bool present
    required property bool isBattery0
    required property bool hasFault
    required property color iconColor
    required property bool isDark

    // BatteryState enum values
    readonly property int bsAsleep: 1
    readonly property int bsIdle: 2
    readonly property int bsActive: 3

    // Charge bar dimensions (scaled from 144x144 to 24x24)
    readonly property real chargeX: 23.0 * (24.0 / 144.0)
    readonly property real chargeY: 41.0 * (24.0 / 144.0)
    readonly property real chargeH: 83.0 * (24.0 / 144.0)
    readonly property real chargeMaxW: 98.0 * (24.0 / 144.0)

    function fillColor(charge, isBattery0) {
        if (isBattery0) {
            if (charge <= 10) return "#FF0000"
            if (charge <= 20) return "#FF7900"
        }
        return ThemeStore.textColor
    }

    // Base battery icon (tinted for theme)
    Image {
        id: baseIcon
        anchors.fill: parent
        source: {
            if (!batteryIcon.present) return "qrc:/ScootUI/assets/icons/librescoot-main-battery-absent.svg"
            if (batteryIcon.batteryState !== bsAsleep && batteryIcon.batteryState !== bsIdle && batteryIcon.charge <= 10)
                return "qrc:/ScootUI/assets/icons/librescoot-main-battery-empty.svg"
            return "qrc:/ScootUI/assets/icons/librescoot-main-battery-blank.svg"
        }
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
        visible: false
    }
    MultiEffect {
        source: baseIcon
        anchors.fill: parent
        colorization: 1.0
        colorizationColor: batteryIcon.iconColor
    }

    // Charge bar (shown for blank icon states)
    Rectangle {
        visible: batteryIcon.present && batteryIcon.charge > 10
                 && (batteryIcon.batteryState === bsActive || batteryIcon.batteryState === bsAsleep || batteryIcon.batteryState === bsIdle || batteryIcon.charge > 10)
        x: chargeX; y: chargeY
        height: chargeH
        width: chargeMaxW * (batteryIcon.charge / 100.0)
        color: fillColor(batteryIcon.charge, batteryIcon.isBattery0)
    }

    // Asleep mask (renders in background color to "cut out" areas)
    Image {
        id: asleepMask
        anchors.fill: parent
        visible: false
        source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-mask.svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
    }
    MultiEffect {
        source: asleepMask
        anchors.fill: parent
        visible: batteryIcon.present && batteryIcon.batteryState === bsAsleep
        colorization: 1.0
        colorizationColor: batteryIcon.isDark ? "#000000" : "#FFFFFF"
    }

    // Asleep overlay
    Image {
        id: asleepOverlay
        anchors.fill: parent
        visible: false
        source: "qrc:/ScootUI/assets/icons/librescoot-main-battery-asleep-overlay.svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
    }
    MultiEffect {
        source: asleepOverlay
        anchors.fill: parent
        visible: batteryIcon.present && batteryIcon.batteryState === bsAsleep
        colorization: 1.0
        colorizationColor: batteryIcon.iconColor
    }

    // Idle overlay (uses original colors in dark mode, inverted in light)
    Image {
        anchors.fill: parent
        visible: batteryIcon.present && batteryIcon.batteryState === bsIdle
        source: batteryIcon.isDark ? "qrc:/ScootUI/assets/icons/librescoot-overlay-idle.svg"
                       : "qrc:/ScootUI/assets/icons/librescoot-overlay-idle-light.svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
    }

    // Fault overlay (uses original colors in dark mode, inverted in light)
    Image {
        anchors.fill: parent
        visible: batteryIcon.hasFault
        source: batteryIcon.isDark ? "qrc:/ScootUI/assets/icons/librescoot-overlay-error.svg"
                       : "qrc:/ScootUI/assets/icons/librescoot-overlay-error-light.svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
    }
}
