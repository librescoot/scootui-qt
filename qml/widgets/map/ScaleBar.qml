import QtQuick

Item {
    id: scaleBar
    width: barWidth
    height: 16

    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    property real zoom: typeof mapService !== "undefined" ? mapService.mapZoom : 17
    property real latitude: typeof mapService !== "undefined" ? mapService.mapLatitude : 52

    readonly property real metersPerPixel: (40075000 * Math.cos(latitude * Math.PI / 180)) / (256 * Math.pow(2, zoom))
    readonly property real maxWidthPx: 160

    // Standard scale values
    readonly property var scaleSteps: [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000]

    function computeScale() {
        var targetMeters = metersPerPixel * maxWidthPx
        for (var i = 0; i < scaleSteps.length; i++) {
            if (scaleSteps[i] >= targetMeters * 0.6) {
                return scaleSteps[i]
            }
        }
        return scaleSteps[scaleSteps.length - 1]
    }

    readonly property int scaleMeters: computeScale()
    readonly property real barWidth: Math.min(Math.max(scaleMeters / metersPerPixel, 40), maxWidthPx)
    readonly property string scaleText: scaleMeters >= 1000
        ? (scaleMeters / 1000) + " km"
        : scaleMeters + " m"
    readonly property color barColor: isDark ? "#9E9E9E" : "#616161"

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var bw = scaleBar.barWidth
            var barH = 8

            // Scale bar (⊥──⊥ shape)
            ctx.strokeStyle = Qt.colorEqual(scaleBar.barColor, scaleBar.barColor) ? scaleBar.barColor : "#9E9E9E"
            ctx.lineWidth = 2

            // Left vertical tick
            ctx.beginPath()
            ctx.moveTo(1, height)
            ctx.lineTo(1, height - barH)
            ctx.stroke()

            // Bottom horizontal bar
            ctx.beginPath()
            ctx.moveTo(1, height - 1)
            ctx.lineTo(bw - 1, height - 1)
            ctx.stroke()

            // Right vertical tick
            ctx.beginPath()
            ctx.moveTo(bw - 1, height)
            ctx.lineTo(bw - 1, height - barH)
            ctx.stroke()
        }
    }

    onZoomChanged: canvas.requestPaint()
    onLatitudeChanged: canvas.requestPaint()
    onIsDarkChanged: canvas.requestPaint()

    // Text with outline centered above the bar
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        text: scaleBar.scaleText
        color: scaleBar.barColor
        font.pixelSize: 10
        font.bold: true
        style: Text.Outline
        styleColor: isDark ? "black" : "white"
    }
}
