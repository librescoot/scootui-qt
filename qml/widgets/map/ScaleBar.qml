import QtQuick
import QtQuick.Shapes
import ScootUI 1.0

Item {
    id: scaleBar
    width: barWidth
    height: 16

    property bool isDark: ThemeStore.isDark
    property real zoom: MapService.mapZoom
    property real latitude: MapService.mapLatitude

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

    // The bracket, drawn with Shapes rather than Canvas so the geometry and the
    // colour are plain bindings. That also drops the three requestPaint handlers
    // this needed to track zoom, latitude and theme.
    Shape {
        id: bracket
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real barH: 8

        // Left tick
        ShapePath {
            strokeColor: scaleBar.barColor
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            startX: 1
            startY: bracket.height
            PathLine { x: 1; y: bracket.height - bracket.barH }
        }

        // Bottom bar
        ShapePath {
            strokeColor: scaleBar.barColor
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            startX: 1
            startY: bracket.height - 1
            PathLine { x: scaleBar.barWidth - 1; y: bracket.height - 1 }
        }

        // Right tick
        ShapePath {
            strokeColor: scaleBar.barColor
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            startX: scaleBar.barWidth - 1
            startY: bracket.height
            PathLine { x: scaleBar.barWidth - 1; y: bracket.height - bracket.barH }
        }
    }

    // Text with outline centered above the bar
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        text: scaleBar.scaleText
        color: scaleBar.barColor
        font.pixelSize: ThemeStore.fontCaption
        font.weight: Font.Bold
        style: Text.Outline
        styleColor: scaleBar.isDark ? "black" : "white"
    }
}
