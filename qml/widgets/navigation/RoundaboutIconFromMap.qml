import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property real size: 64
    property bool isDark: true
    property var renderData: null  // {centerLat, centerLon, bearingDeg, path, arcPath}
    property real bboxMeters: 200  // ~60m roundabout + ~70m of each exit stub

    width: size
    height: size

    // When streets lookup is empty or no renderData, fall through to the
    // generic RoundaboutIcon via this flag.
    readonly property bool hasMap: renderData !== null
                                    && renderData.path !== undefined
                                    && (renderData.path.length > 1)
                                    && visibleFeatures.length > 0

    property var streetFeatures: []
    property var visibleFeatures: []

    onRenderDataChanged: { reloadStreets(); filterFeatures() }
    onStreetFeaturesChanged: filterFeatures()
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
        var latDelta = (bboxMeters / 2) / 111320
        var lonDelta = (bboxMeters / 2) / (111320 * Math.cos(lat * Math.PI / 180))
        streetFeatures = roadInfoService.streetsInBbox(lat - latDelta, lon - lonDelta,
                                                        lat + latDelta, lon + lonDelta)
    }

    // Graph walk from the ring arc: seed with every street whose endpoint
    // lies within a few metres of an on-ring arc waypoint, then repeatedly
    // expand the set by adding any street that shares an endpoint with one
    // already in the set. Bounded to a few iterations so we catch the whole
    // ring (often split into several MVT segments sharing nodes) and the
    // immediate exits, without walking off into unrelated streets.
    function filterFeatures() {
        if (!streetFeatures || streetFeatures.length === 0) {
            visibleFeatures = []
            return
        }
        var arc = renderData ? renderData.arcPath : null
        if (!arc || arc.length === 0) {
            visibleFeatures = streetFeatures
            return
        }
        var cosLat = Math.cos(renderData.centerLat * Math.PI / 180)
        var tol = 3.0
        var tol2 = tol * tol

        function distSq(a, b) {
            var dE = (a[1] - b[1]) * 111320 * cosLat
            var dN = (a[0] - b[0]) * 111320
            return dE * dE + dN * dN
        }

        var n = streetFeatures.length
        var ends = new Array(n)
        for (var f = 0; f < n; f++) {
            var pts = streetFeatures[f].points
            if (pts && pts.length > 0) {
                ends[f] = [pts[0], pts[pts.length - 1]]
            }
        }

        var connected = new Array(n)
        for (var f = 0; f < n; f++) connected[f] = false

        // Seed: features whose endpoints sit on an arc waypoint.
        for (var f = 0; f < n; f++) {
            if (!ends[f]) continue
            for (var e = 0; e < 2 && !connected[f]; e++) {
                for (var i = 0; i < arc.length; i++) {
                    if (distSq(ends[f][e], arc[i]) < tol2) {
                        connected[f] = true
                        break
                    }
                }
            }
        }

        // Expand: any feature sharing an endpoint with a connected feature
        // joins the set. Max 3 passes — enough for the ring + one hop.
        for (var pass = 0; pass < 3; pass++) {
            var changed = false
            for (var f = 0; f < n; f++) {
                if (connected[f] || !ends[f]) continue
                for (var g = 0; g < n && !connected[f]; g++) {
                    if (!connected[g] || !ends[g]) continue
                    for (var ef = 0; ef < 2 && !connected[f]; ef++) {
                        for (var eg = 0; eg < 2; eg++) {
                            if (distSq(ends[f][ef], ends[g][eg]) < tol2) {
                                connected[f] = true
                                changed = true
                                break
                            }
                        }
                    }
                }
            }
            if (!changed) break
        }

        var kept = []
        for (var f = 0; f < n; f++) {
            if (connected[f]) kept.push(streetFeatures[f])
        }
        visibleFeatures = kept
    }

    // Computes the arrow head geometry at the far end of the route:
    //   tip  — the point on the canvas boundary where the polyline exits
    //          (or the actual last waypoint if it fits inside the canvas)
    //   base — aLen back along the path direction from tip
    //   ux/uy — unit direction from base to tip
    // The polyline terminates at `base` so the triangle becomes the visible
    // endpoint. Returns null when the path is too short or degenerate.
    function arrowEnd() {
        if (!renderData || !renderData.path || renderData.path.length < 2) return null
        var p = renderData.path
        var w = size, h = size
        var tip, prev
        var found = false
        for (var i = p.length - 1; i >= 1; i--) {
            var pa = project(p[i - 1][0], p[i - 1][1])
            var pb = project(p[i][0], p[i][1])
            var aIn = pa.x > 0 && pa.x < w && pa.y > 0 && pa.y < h
            var bIn = pb.x > 0 && pb.x < w && pb.y > 0 && pb.y < h
            if (aIn && !bIn) {
                var dx0 = pb.x - pa.x, dy0 = pb.y - pa.y
                var t = 1
                if (dx0 > 0)      t = Math.min(t, (w - pa.x) / dx0)
                else if (dx0 < 0) t = Math.min(t, -pa.x / dx0)
                if (dy0 > 0)      t = Math.min(t, (h - pa.y) / dy0)
                else if (dy0 < 0) t = Math.min(t, -pa.y / dy0)
                t = Math.max(0, Math.min(1, t))
                tip = Qt.point(pa.x + t * dx0, pa.y + t * dy0)
                prev = pa
                found = true
                break
            }
        }
        if (!found) {
            tip = project(p[p.length - 1][0], p[p.length - 1][1])
            prev = project(p[p.length - 2][0], p[p.length - 2][1])
            if (tip.x <= 0 || tip.x >= w || tip.y <= 0 || tip.y >= h) return null
        }
        var dx = tip.x - prev.x, dy = tip.y - prev.y
        var len = Math.sqrt(dx * dx + dy * dy)
        if (len < 1) return null
        var ux = dx / len, uy = dy / len
        var aLen = 13, aWid = 10
        var base = Qt.point(tip.x - aLen * ux, tip.y - aLen * uy)
        return { tip: tip, base: base, ux: ux, uy: uy, aLen: aLen, aWid: aWid }
    }

    // Polyline points truncated at the arrow base: include every projected
    // path waypoint that still sits before the base along the path direction,
    // then finish at the base itself. When arrowEnd() can't be computed,
    // fall back to the full projected path.
    function polylinePoints() {
        if (!renderData || !renderData.path) return []
        var p = renderData.path
        var ae = arrowEnd()
        if (!ae) {
            var allPts = []
            for (var k = 0; k < p.length; k++) {
                allPts.push(project(p[k][0], p[k][1]))
            }
            return allPts
        }
        var pts = []
        for (var i = 0; i < p.length; i++) {
            var pt = project(p[i][0], p[i][1])
            var ahead = (pt.x - ae.base.x) * ae.ux + (pt.y - ae.base.y) * ae.uy
            if (ahead >= 0) break
            pts.push(pt)
        }
        pts.push(ae.base)
        return pts
    }

    function project(lat, lon) {
        if (!renderData) return Qt.point(0, 0)
        var dLat = lat - renderData.centerLat
        var dLon = lon - renderData.centerLon
        var eastM = dLon * 111320 * Math.cos(renderData.centerLat * Math.PI / 180)
        var northM = dLat * 111320
        // Heading-up: rotate so the approach road's forward direction sits
        // at screen-top; bearingDeg comes from the C++ side already prepared.
        var b = (renderData.bearingDeg || 0) * Math.PI / 180
        var cosB = Math.cos(b), sinB = Math.sin(b)
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
            model: root.visibleFeatures
            delegate: Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                property var feat: modelData
                ShapePath {
                    strokeColor: root.isDark ? "#808080" : "#b0b0b0"
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
            preferredRendererType: Shape.CurveRenderer
            visible: root.renderData && root.renderData.path && root.renderData.path.length > 1
            ShapePath {
                strokeColor: root.isDark ? "white" : "#212121"
                strokeWidth: 5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: {
                    var pts = root.polylinePoints()
                    return pts.length > 0 ? pts[0].x : 0
                }
                startY: {
                    var pts = root.polylinePoints()
                    return pts.length > 0 ? pts[0].y : 0
                }
                PathPolyline {
                    path: root.polylinePoints()
                }
            }
        }
    }

    // Arrow head at the exit end of the route. Sits outside mapLayer so the
    // clip isn't in the chain — the Canvas paints its own filled triangle.
    Canvas {
        id: arrow
        anchors.fill: parent
        antialiasing: true
        smooth: true
        z: 10
        visible: root.hasMap
                 && root.renderData && root.renderData.path
                 && root.renderData.path.length >= 2

        Connections {
            target: root
            function onRenderDataChanged() { arrow.requestPaint() }
            function onStreetFeaturesChanged() { arrow.requestPaint() }
            function onVisibleFeaturesChanged() { arrow.requestPaint() }
            function onIsDarkChanged() { arrow.requestPaint() }
        }
        Component.onCompleted: requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onAvailableChanged: if (available) requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var ae = root.arrowEnd()
            if (!ae) return
            ctx.beginPath()
            ctx.moveTo(ae.tip.x, ae.tip.y)
            ctx.lineTo(ae.base.x + ae.aWid * ae.uy, ae.base.y - ae.aWid * ae.ux)
            ctx.lineTo(ae.base.x - ae.aWid * ae.uy, ae.base.y + ae.aWid * ae.ux)
            ctx.closePath()
            ctx.fillStyle = root.isDark ? "white" : "#212121"
            ctx.fill()
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
