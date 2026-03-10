import QtQuick

Item {
    id: scaleBar
    width: 100
    height: 24

    property real zoom: typeof mapService !== "undefined" ? mapService.mapZoom : 16
    property real latitude: typeof mapService !== "undefined" ? mapService.mapLatitude : 52

    readonly property real metersPerPixel: (40075000 * Math.cos(latitude * Math.PI / 180)) / (256 * Math.pow(2, zoom))
    readonly property real maxWidthPx: 100

    // Standard scale values
    readonly property var scaleSteps: [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000]

    function computeScale() {
        var targetMeters = metersPerPixel * maxWidthPx
        for (var i = 0; i < scaleSteps.length; i++) {
            if (scaleSteps[i] >= targetMeters * 0.3) {
                return scaleSteps[i]
            }
        }
        return scaleSteps[scaleSteps.length - 1]
    }

    readonly property int scaleMeters: computeScale()
    readonly property real barWidth: scaleMeters / metersPerPixel
    readonly property string scaleText: scaleMeters >= 1000
        ? (scaleMeters / 1000) + " km"
        : scaleMeters + " m"

    Canvas {
        width: Math.min(scaleBar.barWidth + 10, scaleBar.maxWidthPx)
        height: scaleBar.height
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var bw = scaleBar.barWidth
            var x0 = 2
            var y = height - 4

            // Scale bar (L-shape)
            ctx.strokeStyle = "white"
            ctx.lineWidth = 2

            // Bottom line
            ctx.beginPath()
            ctx.moveTo(x0, y)
            ctx.lineTo(x0 + bw, y)
            ctx.stroke()

            // Left tick
            ctx.beginPath()
            ctx.moveTo(x0, y)
            ctx.lineTo(x0, y - 6)
            ctx.stroke()

            // Right tick
            ctx.beginPath()
            ctx.moveTo(x0 + bw, y)
            ctx.lineTo(x0 + bw, y - 6)
            ctx.stroke()
        }

        onZoomChanged: requestPaint()
        onLatitudeChanged: requestPaint()
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.rightMargin: 4
        text: scaleBar.scaleText
        color: "white"
        font.pixelSize: 10
        style: Text.Outline
        styleColor: Qt.rgba(0, 0, 0, 0.5)
    }
}
