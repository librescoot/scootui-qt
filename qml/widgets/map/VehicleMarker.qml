import QtQuick

Item {
    id: vehicleMarker
    width: 100
    height: 100

    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    property bool isNavigating: typeof navigationService !== "undefined" ? navigationService.isNavigating : false
    property bool hasRecentFix: typeof gpsStore !== "undefined" ? gpsStore.hasRecentFix : true
    property double eph: typeof gpsStore !== "undefined" ? gpsStore.eph : 10

    // Scale eph (meters) to a circle diameter in pixels.
    // At eph=5 → tight ~30px, at eph=50 → large ~60px. Clamped.
    readonly property real ephCircleSize: Math.max(30, Math.min(60, 20 + eph * 0.8))

    // When on DR, use a fixed large circle regardless of eph
    readonly property real drCircleSize: 70

    // Active circle size depends on GPS state
    readonly property real activeCircleSize: hasRecentFix ? ephCircleSize : drCircleSize

    // Colors
    readonly property color arrowColor: hasRecentFix ? "#2196F3" : "#888888"
    readonly property color circleColor: hasRecentFix
        ? (isDark ? Qt.rgba(0.26, 0.26, 0.26, 0.7) : Qt.rgba(0.88, 0.88, 0.88, 0.7))
        : Qt.rgba(0.5, 0.5, 0.5, 0.5)
    readonly property color circleBorderColor: hasRecentFix
        ? (isDark ? Qt.rgba(0.46, 0.46, 0.46, 0.9) : Qt.rgba(0.62, 0.62, 0.62, 0.9))
        : Qt.rgba(0.6, 0.6, 0.6, 0.6)

    // Sonar ring 1
    Rectangle {
        id: sonarRing1
        anchors.centerIn: parent
        width: activeCircleSize
        height: activeCircleSize
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.6)
        opacity: 0
        visible: !hasRecentFix

        SequentialAnimation on opacity {
            running: !hasRecentFix
            loops: Animation.Infinite
            NumberAnimation { from: 0.6; to: 0; duration: 2000; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 500 }
        }

        SequentialAnimation on scale {
            running: !hasRecentFix
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 2.2; duration: 2000; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 500 }
        }
    }

    // Sonar ring 2 (staggered by 1000ms)
    Rectangle {
        id: sonarRing2
        anchors.centerIn: parent
        width: activeCircleSize
        height: activeCircleSize
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.6)
        opacity: 0
        visible: !hasRecentFix

        SequentialAnimation on opacity {
            running: !hasRecentFix
            loops: Animation.Infinite
            PauseAnimation { duration: 1000 }
            NumberAnimation { from: 0.6; to: 0; duration: 2000; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 500 }
        }

        SequentialAnimation on scale {
            running: !hasRecentFix
            loops: Animation.Infinite
            PauseAnimation { duration: 1000 }
            NumberAnimation { from: 1.0; to: 2.2; duration: 2000; easing.type: Easing.OutQuad }
            PauseAnimation { duration: 500 }
        }
    }

    // Inner circle
    Rectangle {
        anchors.centerIn: parent
        width: activeCircleSize
        height: activeCircleSize
        radius: width / 2
        color: circleColor
        border.width: 1
        border.color: circleBorderColor

        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on color { ColorAnimation { duration: 300 } }
        Behavior on border.color { ColorAnimation { duration: 300 } }

        // Navigation arrow
        Canvas {
            anchors.centerIn: parent
            width: 24
            height: 24

            property color fillColor: vehicleMarker.arrowColor

            onFillColorChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = fillColor
                ctx.beginPath()
                ctx.moveTo(12, 2)
                ctx.lineTo(20, 20)
                ctx.lineTo(12, 15)
                ctx.lineTo(4, 20)
                ctx.closePath()
                ctx.fill()
            }
        }
    }
}
