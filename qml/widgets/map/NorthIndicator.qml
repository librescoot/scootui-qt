import QtQuick
import "../../theme"

Item {
    id: northIndicator
    width: 24
    height: 24

    property real bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0
    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusModal
        color: isDark ? Qt.rgba(0.31, 0.31, 0.31, 0.9) : Qt.rgba(0.76, 0.76, 0.76, 0.9)
        border.width: 0.5
        border.color: isDark ? Qt.rgba(0.46, 0.46, 0.46, 0.9) : Qt.rgba(0.62, 0.62, 0.62, 0.9)

        Canvas {
            anchors.fill: parent
            rotation: -northIndicator.bearing

            Behavior on rotation {
                RotationAnimation {
                    duration: 300
                    direction: RotationAnimation.Shortest
                    easing.type: Easing.InOutQuad
                }
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var cx = width / 2
                var cy = height / 2
                var r = width / 2

                // North needle (red)
                ctx.fillStyle = "#FF0000"
                ctx.beginPath()
                ctx.moveTo(cx, cy - r * 0.8)
                ctx.lineTo(cx + r * 0.3, cy)
                ctx.lineTo(cx - r * 0.3, cy)
                ctx.closePath()
                ctx.fill()

                // South needle (gray)
                ctx.fillStyle = northIndicator.isDark ? "#9E9E9E" : "#757575"
                ctx.beginPath()
                ctx.moveTo(cx, cy + r * 0.8)
                ctx.lineTo(cx + r * 0.3, cy)
                ctx.lineTo(cx - r * 0.3, cy)
                ctx.closePath()
                ctx.fill()
            }
        }
    }
}
