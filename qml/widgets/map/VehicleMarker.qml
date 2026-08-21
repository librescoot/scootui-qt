import QtQuick
import QtQuick.Shapes
import ScootUI 1.0

Item {
    id: vehicleMarker
    width: 100
    height: 100

    property bool isDark: typeof ThemeStore !== "undefined" ? ThemeStore.isDark : true
    property bool isNavigating: typeof NavigationService !== "undefined" ? NavigationService.isNavigating : false
    property bool hasRecentFix: typeof GpsStore !== "undefined" ? GpsStore.hasRecentFix : true
    property double eph: typeof GpsStore !== "undefined" ? GpsStore.eph : 10

    // Screen rotation of the direction arrow (deg clockwise from up).
    // The map rotates by MapService.mapBearing; the arrow must always point
    // along the true heading, which in north-oriented 2D is NOT "up" (the map
    // stays fixed there). Difference of the raw heading and the effective map
    // rotation handles every mode: direction/3D -> the map already turns so the
    // arrow stays up (0), north-2D -> arrow turns to the heading.
    readonly property real headingAngle: typeof MapService !== "undefined"
                                         ? MapService.rawMapBearing - MapService.mapBearing : 0
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
        // The heading arrow. Shapes rather than Canvas: fillColor is a live
        // binding, so the onFillColorChanged/requestPaint pair is gone too.
        Shape {
            anchors.centerIn: parent
            width: 24
            height: 24
            preferredRendererType: Shape.CurveRenderer

            // Points along the true heading (north-oriented 2D) / up in
            // direction-oriented and 3D views.
            transform: Rotation {
                origin.x: 12
                origin.y: 12
                angle: vehicleMarker.headingAngle
            }

            ShapePath {
                fillColor: vehicleMarker.arrowColor
                strokeColor: "transparent"
                startX: 12
                startY: 2
                PathLine { x: 20; y: 20 }
                PathLine { x: 12; y: 15 }
                PathLine { x: 4;  y: 20 }
                PathLine { x: 12; y: 2 }
            }
        }
    }
}
