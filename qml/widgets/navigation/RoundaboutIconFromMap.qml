import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property real size: 64
    property bool isDark: true
    property var renderData: null  // {centerLat, centerLon, bearingDeg, path}
    property real bboxMeters: 100  // ~50m roundabout + 25m of each exit stub

    width: size
    height: size

    // When streets lookup is empty or no renderData, fall through to the
    // generic RoundaboutIcon via this flag.
    readonly property bool hasMap: renderData !== null
                                    && renderData.path !== undefined
                                    && (renderData.path.length > 1)
                                    && streetFeatures.length > 0

    property var streetFeatures: []

    onRenderDataChanged: reloadStreets()
    Component.onCompleted: reloadStreets()

    function reloadStreets() {
        if (!renderData || renderData.centerLat === undefined) {
            streetFeatures = []
            return
        }
        if (typeof roadInfoService === "undefined") {
            streetFeatures = []
            return
        }
        var lat = renderData.centerLat
        var lon = renderData.centerLon
        // 1° lat ~ 111320 m; 1° lon ~ 111320 * cos(lat)
        var latDelta = (bboxMeters / 2) / 111320
        var lonDelta = (bboxMeters / 2) / (111320 * Math.cos(lat * Math.PI / 180))
        streetFeatures = roadInfoService.streetsInBbox(lat - latDelta, lon - lonDelta,
                                                        lat + latDelta, lon + lonDelta)
    }

    function project(lat, lon) {
        // Flat projection is fine for 100m windows.
        if (!renderData) return Qt.point(0, 0)
        var dLat = lat - renderData.centerLat
        var dLon = lon - renderData.centerLon
        var eastM = dLon * 111320 * Math.cos(renderData.centerLat * Math.PI / 180)
        var northM = dLat * 111320
        // Rotate so bearing points up. Bearing is clockwise-from-north in deg.
        var b = (renderData.bearingDeg || 0) * Math.PI / 180
        var cosB = Math.cos(b), sinB = Math.sin(b)
        // Rotate (east, north) by +bearing so the approach vector points up.
        var rotE = eastM * cosB - northM * sinB
        var rotN = eastM * sinB + northM * cosB
        var pxPerMeter = size / bboxMeters
        var px = rotE * pxPerMeter + size / 2
        var py = -rotN * pxPerMeter + size / 2
        return Qt.point(px, py)
    }

    Item {
        id: mapLayer
        anchors.fill: parent
        visible: root.hasMap
        clip: true

        // Grey streets
        Repeater {
            model: root.streetFeatures
            delegate: Shape {
                anchors.fill: parent
                antialiasing: true
                property var feat: modelData
                ShapePath {
                    strokeColor: root.isDark ? "#606060" : "#a0a0a0"
                    strokeWidth: (feat && (feat.kind === "primary" || feat.kind === "secondary")) ? 5 : 3
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: (feat && feat.points && feat.points.length > 0)
                            ? root.project(feat.points[0][0], feat.points[0][1]).x : 0
                    startY: (feat && feat.points && feat.points.length > 0)
                            ? root.project(feat.points[0][0], feat.points[0][1]).y : 0
                    PathPolyline {
                        path: {
                            var pts = []
                            if (feat && feat.points) {
                                for (var i = 0; i < feat.points.length; ++i) {
                                    pts.push(root.project(feat.points[i][0], feat.points[i][1]))
                                }
                            }
                            return pts
                        }
                    }
                }
            }
        }

        // White route overlay
        Shape {
            anchors.fill: parent
            antialiasing: true
            visible: root.renderData && root.renderData.path && root.renderData.path.length > 1
            ShapePath {
                strokeColor: root.isDark ? "white" : "#212121"
                strokeWidth: 5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: root.renderData && root.renderData.path && root.renderData.path.length > 0
                        ? root.project(root.renderData.path[0][0], root.renderData.path[0][1]).x : 0
                startY: root.renderData && root.renderData.path && root.renderData.path.length > 0
                        ? root.project(root.renderData.path[0][0], root.renderData.path[0][1]).y : 0
                PathPolyline {
                    path: {
                        var pts = []
                        if (root.renderData && root.renderData.path) {
                            for (var i = 0; i < root.renderData.path.length; ++i) {
                                pts.push(root.project(root.renderData.path[i][0], root.renderData.path[i][1]))
                            }
                        }
                        return pts
                    }
                }
            }
        }
    }

    // Fallback: generic rose
    RoundaboutIcon {
        anchors.centerIn: parent
        visible: !root.hasMap
        exitNumber: typeof navigationService !== "undefined"
                    ? Math.max(1, navigationService.roundaboutExitCount) : 1
        isDark: root.isDark
        size: root.size
    }
}
