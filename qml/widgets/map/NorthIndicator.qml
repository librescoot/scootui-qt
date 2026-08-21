import QtQuick
import QtQuick.Shapes
import ScootUI 1.0

Item {
    id: northIndicator
    width: 24
    height: 24

    property real bearing: MapService.mapBearing
    property bool isDark: ThemeStore.isDark

    Rectangle {
        anchors.fill: parent
        radius: ThemeStore.radiusModal
        color: northIndicator.isDark ? Qt.rgba(0.31, 0.31, 0.31, 0.9) : Qt.rgba(0.76, 0.76, 0.76, 0.9)
        border.width: 0.5
        border.color: northIndicator.isDark ? Qt.rgba(0.46, 0.46, 0.46, 0.9) : Qt.rgba(0.62, 0.62, 0.62, 0.9)

        // Two needles meeting at the centre. Shapes rather than Canvas: the fill
        // colours are live bindings, so the south needle now follows a theme
        // change on its own. The Canvas version only repainted when its size
        // changed, so it kept the boot-time colour.
        Shape {
            id: needles
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            rotation: -northIndicator.bearing

            readonly property real cx: width / 2
            readonly property real cy: height / 2
            readonly property real r: width / 2

            Behavior on rotation {
                RotationAnimation {
                    duration: 300
                    direction: RotationAnimation.Shortest
                    easing.type: Easing.InOutQuad
                }
            }

            ShapePath {
                fillColor: "#FF0000"
                strokeColor: "transparent"
                startX: needles.cx
                startY: needles.cy - needles.r * 0.8
                PathLine { x: needles.cx + needles.r * 0.3; y: needles.cy }
                PathLine { x: needles.cx - needles.r * 0.3; y: needles.cy }
                PathLine { x: needles.cx; y: needles.cy - needles.r * 0.8 }
            }

            ShapePath {
                fillColor: northIndicator.isDark ? "#9E9E9E" : "#757575"
                strokeColor: "transparent"
                startX: needles.cx
                startY: needles.cy + needles.r * 0.8
                PathLine { x: needles.cx + needles.r * 0.3; y: needles.cy }
                PathLine { x: needles.cx - needles.r * 0.3; y: needles.cy }
                PathLine { x: needles.cx; y: needles.cy + needles.r * 0.8 }
            }
        }
    }
}
