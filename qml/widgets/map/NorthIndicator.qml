import QtQuick

Item {
    id: northIndicator
    width: 32
    height: 32

    property real bearing: typeof mapService !== "undefined" ? mapService.mapBearing : 0

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

            // North needle (red)
            ctx.fillStyle = "#FF4444"
            ctx.beginPath()
            ctx.moveTo(cx, 4)
            ctx.lineTo(cx + 5, cy)
            ctx.lineTo(cx - 5, cy)
            ctx.closePath()
            ctx.fill()

            // South needle (gray)
            ctx.fillStyle = "#888888"
            ctx.beginPath()
            ctx.moveTo(cx, height - 4)
            ctx.lineTo(cx + 5, cy)
            ctx.lineTo(cx - 5, cy)
            ctx.closePath()
            ctx.fill()

            // Center circle
            ctx.fillStyle = "white"
            ctx.beginPath()
            ctx.arc(cx, cy, 3, 0, 2 * Math.PI)
            ctx.fill()
        }
    }
}
