import QtQuick
import QtQuick.Shapes

// Roundabout icon drawn from real map geometry: a clean ring at a size that
// does not depend on how big the roundabout is, the arms that actually meet
// it, and the route through it entering from the bottom.
//
// Everything is projected once per renderData change into `layout`, which holds
// nothing but pixel coordinates. The render tree below only reads that, so a
// repaint never re-runs the geometry.
Item {
    id: root

    property real size: 80
    property bool isDark: true
    property var renderData: null

    width: size
    height: size

    readonly property color inkColor: isDark ? "white" : "#212121"
    readonly property color roadColor: isDark ? "#808080" : "#b0b0b0"

    // The item fills its 80 px cell. The inset only has to cover stroke
    // half-widths, because the arrow head gets its room reserved where it
    // actually sits rather than on every side, and the arms are cut to stubs
    // that stop short of the frame.
    readonly property real margin: 6
    readonly property real routeWidth: 5
    // Open chevron, same pen weight as the shaft, as the other maneuver icons
    // draw it. The wings need this much room from the shaft or they close up
    // into a solid blob at a 5 px stroke.
    readonly property real arrowLen: 9
    readonly property real arrowWidth: 8

    property var layout: null
    readonly property bool hasMap: layout !== null

    onRenderDataChanged: rebuild()
    onSizeChanged: rebuild()
    Component.onCompleted: rebuild()

    function rebuild() {
        layout = computeLayout()
        arrowCanvas.requestPaint()
    }

    // --- small geometric helpers, all in a local east/north metre frame ---

    function metresPerLon(lat) { return 111320 * Math.cos(lat * Math.PI / 180) }

    function bearingDeg(fromLat, fromLon, toLat, toLon) {
        var dE = (toLon - fromLon) * metresPerLon(fromLat)
        var dN = (toLat - fromLat) * 111320
        return Math.atan2(dE, dN) * 180 / Math.PI
    }

    // Kasa least-squares circle. Linear in (cx, cy, c), so a 3x3 solve. Returns
    // null when the normal equations are singular.
    function fitCircle(pts) {
        if (!pts || pts.length < 3) return null
        var lat0 = pts[0][0], lon0 = pts[0][1]
        var mPerLon = metresPerLon(lat0)
        var sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0, sxz = 0, syz = 0, sz = 0
        var n = pts.length
        for (var i = 0; i < n; i++) {
            var x = (pts[i][1] - lon0) * mPerLon
            var y = (pts[i][0] - lat0) * 111320
            var z = x * x + y * y
            sx += x; sy += y
            sxx += x * x; syy += y * y; sxy += x * y
            sxz += x * z; syz += y * z; sz += z
        }
        var m = [[sxx, sxy, sx], [sxy, syy, sy], [sx, sy, n]]
        var v = [sxz / 2, syz / 2, sz / 2]
        function det3(a) {
            return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
                 - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                 + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0])
        }
        var det = det3(m)
        if (Math.abs(det) < 1e-9) return null
        var sol = []
        for (var c = 0; c < 3; c++) {
            var t = []
            for (var r = 0; r < 3; r++) {
                t.push([c === 0 ? v[r] : m[r][0],
                        c === 1 ? v[r] : m[r][1],
                        c === 2 ? v[r] : m[r][2]])
            }
            sol.push(det3(t) / det)
        }
        var cx = sol[0], cy = sol[1]
        var rSq = sol[2] + cx * cx + cy * cy
        if (!(rSq > 0)) return null
        return { lat: lat0 + cy / 111320,
                 lon: lon0 + cx / mPerLon,
                 r: Math.sqrt(rSq) }
    }

    function fetchStreets(lat, lon, reachM) {
        if (typeof roadInfoService === "undefined") return []
        var dLat = reachM / 111320
        var dLon = reachM / metresPerLon(lat)
        return roadInfoService.streetsInBbox(lat - dLat, lon - dLon,
                                             lat + dLat, lon + dLon)
    }

    // Last-resort ring recovery: the route arc did not pin the circle down, so
    // fit whatever the tile explicitly tagged as a roundabout near the anchor.
    // Covers the small suburban rings, which are exactly the ones whose arcs
    // are too short to fit.
    function ringFromTiles(feats, lat, lon, reachM) {
        var pts = []
        for (var i = 0; i < feats.length; i++) {
            if (!feats[i].roundabout || !feats[i].points) continue
            var fp = feats[i].points
            for (var j = 0; j < fp.length; j++) {
                var dE = (fp[j][1] - lon) * metresPerLon(lat)
                var dN = (fp[j][0] - lat) * 111320
                if (dE * dE + dN * dN <= reachM * reachM) pts.push(fp[j])
            }
        }
        var fit = fitCircle(pts)
        if (!fit || fit.r < 4 || fit.r > 90) return null
        return fit
    }

    // Arms are the roads that reach the ring and carry on outwards. Testing
    // radius against the fitted circle beats walking the street graph: it does
    // not care whether the ring is one closed way or eleven open ones, whether
    // the tile tagged it, or where an arbitrary OSM node happens to sit.
    //
    // Nothing inside the circle is kept. A real Platz kerb is not round (at
    // Bersarinplatz the ring geometry runs 34 m to 61 m off a centre the driven
    // line fits to within 1.4 m), so drawing it under a fitted circle gives two
    // outlines that disagree. The icon is schematic: one true circle for the
    // ring, real bearings for the arms.
    function selectArms(feats, cLat, cLon, R) {
        var mPerLon = metresPerLon(cLat)
        var tol = Math.max(6.0, 0.30 * R)
        var kept = []
        for (var i = 0; i < feats.length; i++) {
            var pts = feats[i].points
            if (!pts || pts.length < 2) continue
            var rmin = 1e9, rmax = 0
            for (var j = 0; j < pts.length; j++) {
                var dE = (pts[j][1] - cLon) * mPerLon
                var dN = (pts[j][0] - cLat) * 111320
                var r = Math.sqrt(dE * dE + dN * dN)
                if (r < rmin) rmin = r
                if (r > rmax) rmax = r
            }
            // Has to start at or inside the kerb band and actually head out.
            if (rmin <= R + tol && rmax > R + 2)
                kept.push(feats[i])
        }
        return kept
    }

    // How much road a kind is worth when two arms leave in the same direction.
    function roadRank(kind) {
        switch (kind) {
        case "motorway": case "trunk": return 6
        case "primary": return 5
        case "secondary": return 4
        case "tertiary": return 3
        case "unclassified": case "residential": return 2
        case "living_street": return 1
        default: return 0
        }
    }

    function polylineLength(pts) {
        var acc = 0
        for (var i = 1; i < pts.length; i++) {
            var dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y
            acc += Math.sqrt(dx * dx + dy * dy)
        }
        return acc
    }

    // Cut a polyline down to at most `maxLen` of its own length.
    function limitRun(pts, maxLen) {
        var out = [pts[0]]
        var acc = 0
        for (var i = 1; i < pts.length; i++) {
            var dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y
            var seg = Math.sqrt(dx * dx + dy * dy)
            if (acc + seg >= maxLen) {
                var t = (maxLen - acc) / seg
                out.push({ x: pts[i - 1].x + dx * t, y: pts[i - 1].y + dy * t })
                return out
            }
            acc += seg
            out.push(pts[i])
        }
        return out
    }

    // Point where segment a->b crosses the circle of radius R about the origin.
    function circleCrossing(a, b, R) {
        var dx = b.x - a.x, dy = b.y - a.y
        var qa = dx * dx + dy * dy
        if (qa < 1e-9) return b
        var qb = 2 * (a.x * dx + a.y * dy)
        var qc = a.x * a.x + a.y * a.y - R * R
        var disc = qb * qb - 4 * qa * qc
        if (disc < 0) return b
        var s = Math.sqrt(disc)
        var t1 = (-qb - s) / (2 * qa), t2 = (-qb + s) / (2 * qa)
        var t = (t1 >= 0 && t1 <= 1) ? t1 : ((t2 >= 0 && t2 <= 1) ? t2 : -1)
        if (t < 0) return b
        return { x: a.x + t * dx, y: a.y + t * dy }
    }

    // Split a polyline into the runs that lie outside the ring, each starting
    // exactly on the circle so the arm meets the kerb cleanly.
    function runsOutside(pts, R) {
        var runs = [], cur = []
        for (var i = 0; i < pts.length; i++) {
            var out = Math.sqrt(pts[i].x * pts[i].x + pts[i].y * pts[i].y) >= R
            if (out) {
                if (cur.length === 0 && i > 0)
                    cur.push(circleCrossing(pts[i - 1], pts[i], R))
                cur.push(pts[i])
            } else if (cur.length > 0) {
                cur.push(circleCrossing(pts[i - 1], pts[i], R))
                if (cur.length >= 2) runs.push(cur)
                cur = []
            }
        }
        if (cur.length >= 2) runs.push(cur)
        return runs
    }

    function computeLayout() {
        var rd = renderData
        if (!rd || rd.centerLat === undefined) return null
        var path = rd.path
        if (!path || path.length < 2) return null
        var entryIdx = rd.entryIndex !== undefined ? rd.entryIndex : 0
        var exitIdx = rd.exitIndex !== undefined ? rd.exitIndex : path.length - 1
        if (entryIdx < 0 || exitIdx >= path.length || exitIdx <= entryIdx) return null

        var cLat = rd.centerLat, cLon = rd.centerLon
        var R = rd.ringRadius || 0
        var valid = rd.ringValid === true

        var reach = valid ? (R + Math.max(25, 0.9 * R))
                          : (Math.max(R, 12) * 3 + 60)
        var feats = fetchStreets(cLat, cLon, reach)

        if (!valid) {
            var refit = ringFromTiles(feats, cLat, cLon, reach)
            if (!refit) return null
            cLat = refit.lat; cLon = refit.lon; R = refit.r
        }
        if (!(R >= 4)) return null

        // The approach is drawn shorter than the exit. An approach road is
        // already bending into the junction well before it gets there: over the
        // full stub at Ernst-Reuter-Platz the outermost fifth alone swings 28
        // degrees off the rest. Keeping only the stretch beside the ring leaves
        // a piece of road that is genuinely straight, which is what lets the
        // rotation below make it vertical rather than merely average out.
        var entry = path[entryIdx]
        var entryStubM = Math.max(12, 0.45 * R)
        var mPerLonE = metresPerLon(entry[0])
        var approachLL = []
        var acc = 0, k = entryIdx
        while (k > 0 && acc < entryStubM) {
            var sdE = (path[k][1] - path[k - 1][1]) * mPerLonE
            var sdN = (path[k][0] - path[k - 1][0]) * 111320
            var segLen = Math.sqrt(sdE * sdE + sdN * sdN)
            if (acc + segLen >= entryStubM) {
                var t = (segLen > 1e-6) ? (entryStubM - acc) / segLen : 0
                approachLL.unshift([path[k][0] + (path[k - 1][0] - path[k][0]) * t,
                                    path[k][1] + (path[k - 1][1] - path[k][1]) * t])
                break
            }
            acc += segLen
            k--
            approachLL.unshift(path[k])
        }

        // Rotate by that stretch's own direction, so the road the rider is on
        // runs straight down out of the ring.
        var back = approachLL.length > 0 ? approachLL[0] : path[0]
        var rot = bearingDeg(back[0], back[1], entry[0], entry[1]) * Math.PI / 180
        var cb = Math.cos(rot), sb = Math.sin(rot)
        var mPerLon = metresPerLon(cLat)
        function toEN(lat, lon) {
            var e = (lon - cLon) * mPerLon
            var nn = (lat - cLat) * 111320
            return { x: e * cb - nn * sb, y: e * sb + nn * cb }
        }

        // Route: the approach and the exit keep their real shape, so the white
        // line sits on the same road the grey arm draws. Only the on-ring arc
        // is snapped onto the fitted circle, so it never drifts off the ring.
        var arcEN = []
        for (var i = entryIdx; i <= exitIdx; i++) {
            var ap0 = toEN(path[i][0], path[i][1])
            var ar = Math.sqrt(ap0.x * ap0.x + ap0.y * ap0.y)
            arcEN.push(ar > 1e-6 ? { x: ap0.x * R / ar, y: ap0.y * R / ar } : ap0)
        }
        var entryOnRing = arcEN[0]

        // One straight segment, dead vertical, from the entry down. The
        // rotation above already put the road on that axis, so this only
        // removes the last degree or so of wobble and the half-pixel the entry
        // moved when it was snapped onto the circle. The grey arm still draws
        // the road's real shape underneath, within about a pixel of it.
        var routeEN = [{ x: entryOnRing.x, y: entryOnRing.y - entryStubM }]
        for (var c2 = 0; c2 < arcEN.length; c2++) routeEN.push(arcEN[c2])

        var exitRaw = toEN(path[exitIdx][0], path[exitIdx][1])
        var exitSnapped = routeEN[routeEN.length - 1]
        var shiftX = exitSnapped.x - exitRaw.x, shiftY = exitSnapped.y - exitRaw.y
        for (var e = exitIdx + 1; e < path.length; e++) {
            var ep = toEN(path[e][0], path[e][1])
            routeEN.push({ x: ep.x + shiftX, y: ep.y + shiftY })
        }

        var arms = selectArms(feats, cLat, cLon, R)
        var cands = []
        for (var a = 0; a < arms.length; a++) {
            var ap = arms[a].points, seq = []
            for (var q = 0; q < ap.length; q++) seq.push(toEN(ap[q][0], ap[q][1]))
            var runs = runsOutside(seq, R)
            for (var rr = 0; rr < runs.length; rr++) {
                var rl = polylineLength(runs[rr])
                // A road that only clips the kerb leaves a stub too short to be
                // a direction. Measured over 184 roundabouts, this fires on one
                // of them, so it removes specks without thinning real arms.
                if (rl < 0.12 * R) continue
                cands.push({ pts: runs[rr], kind: arms[a].kind, len: rl,
                             ang: Math.atan2(runs[rr][0].y, runs[rr][0].x) * 180 / Math.PI })
            }
        }

        // One stub per direction. A dual carriageway reaches the ring as two
        // roads a few degrees apart, and drawing both says "two exits" where
        // there is one. Keeping the most major, then the longest, of each
        // cluster leaves the count a rider sees intact: over the same 184
        // roundabouts this takes Ernst-Reuter-Platz from 24 stubs to 10 while
        // leaving the median roundabout's 4 alone.
        cands.sort(function (p, q) {
            var d = roadRank(q.kind) - roadRank(p.kind)
            return d !== 0 ? d : q.len - p.len
        })
        var armsEN = []
        var taken = []
        for (var c3 = 0; c3 < cands.length; c3++) {
            var clash = false
            for (var t3 = 0; t3 < taken.length && !clash; t3++) {
                var dA = Math.abs(((cands[c3].ang - taken[t3] + 180) % 360 + 360) % 360 - 180)
                if (dA < 18) clash = true
            }
            if (clash) continue
            taken.push(cands[c3].ang)
            // Arms are stubs, not roads followed to wherever they go: the ring
            // and the route carry the meaning, and letting every arm run to the
            // frame edge fills the cell edge to edge and loses the whitespace
            // the glyph icons alongside have.
            armsEN.push({ pts: limitRun(cands[c3].pts, 0.45 * R),
                          w: roadRank(cands[c3].kind) >= 4 ? 5 : 3 })
        }

        // Fit the ring plus the route into the tile. Arms are deliberately
        // excluded: they run off to wherever they go, and framing to them would
        // shrink the roundabout to nothing.
        var minX = -R, maxX = R, minY = -R, maxY = R
        for (var m = 0; m < routeEN.length; m++) {
            minX = Math.min(minX, routeEN[m].x); maxX = Math.max(maxX, routeEN[m].x)
            minY = Math.min(minY, routeEN[m].y); maxY = Math.max(maxY, routeEN[m].y)
        }
        // Two passes. The arrow head's wings reach a fixed number of pixels off
        // the tip, so how much ground they need depends on the scale, which
        // depends on them. Fit once without them, use that to convert the wing
        // reach into metres, widen the box just around the tip, then refit.
        // Reserving that room on all four sides instead costs about a fifth of
        // the ring's diameter for space nothing ever draws into.
        var avail = size - 2 * margin
        function fitTo(x0, x1, y0, y1) {
            return Math.min(avail / Math.max(1e-6, x1 - x0),
                            avail / Math.max(1e-6, y1 - y0))
        }
        var scale = fitTo(minX, maxX, minY, maxY)
        var tipEN = routeEN[routeEN.length - 1]
        var reachM = (arrowWidth + routeWidth / 2 + 1) / scale
        minX = Math.min(minX, tipEN.x - reachM); maxX = Math.max(maxX, tipEN.x + reachM)
        minY = Math.min(minY, tipEN.y - reachM); maxY = Math.max(maxY, tipEN.y + reachM)
        scale = fitTo(minX, maxX, minY, maxY)

        var ox = (size - (maxX + minX) * scale) / 2
        var oy = (size + (maxY + minY) * scale) / 2
        function px(p) { return Qt.point(ox + p.x * scale, oy - p.y * scale) }

        var routePx = []
        for (var t = 0; t < routeEN.length; t++) routePx.push(px(routeEN[t]))
        var armsPx = []
        for (var u = 0; u < armsEN.length; u++) {
            var s = []
            for (var w = 0; w < armsEN[u].pts.length; w++) s.push(px(armsEN[u].pts[w]))
            armsPx.push({ pts: s, w: armsEN[u].w })
        }

        // The arrow head sits at the far end of the exit road, pointing the way
        // the road actually goes. The framing above already guaranteed it fits,
        // so there is no clipping to compensate for.
        // Direction over a real span of the exit, not just the final segment:
        // Valhalla's shape can put its last two points a fraction of a pixel
        // apart, which used to leave the icon with no arrow head at all.
        var tip = routePx[routePx.length - 1]
        var dir = null
        for (var d = routePx.length - 2; d >= 0 && dir === null; d--) {
            var ddx = tip.x - routePx[d].x, ddy = tip.y - routePx[d].y
            var dl = Math.sqrt(ddx * ddx + ddy * ddy)
            if (dl >= 6) dir = { ux: ddx / dl, uy: ddy / dl }
        }
        var arrow = null
        if (dir) {
            // The shaft runs all the way into the vertex and the wings spread
            // back from it, so the whole arrow reads as one continuous stroke.
            arrow = { x: tip.x, y: tip.y, ux: dir.ux, uy: dir.uy }
        }

        var centre = px({ x: 0, y: 0 })
        return { ring: { cx: centre.x, cy: centre.y, r: R * scale },
                 arms: armsPx, route: routePx, arrow: arrow }
    }

    Item {
        anchors.fill: parent
        visible: root.hasMap
        clip: true

        // Arms first, then the ring on top of them, so the kerb line reads as
        // continuous where an arm runs up to it.
        Repeater {
            model: root.layout ? root.layout.arms : []
            delegate: Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                required property var modelData
                ShapePath {
                    strokeColor: root.roadColor
                    strokeWidth: modelData.w
                    fillColor: "transparent"
                    // Flat: an arm is cut off mid-road, and a round cap turns
                    // that cut into a dome that reads as the road's own end.
                    capStyle: ShapePath.FlatCap
                    joinStyle: ShapePath.RoundJoin
                    startX: modelData.pts.length > 0 ? modelData.pts[0].x : 0
                    startY: modelData.pts.length > 0 ? modelData.pts[0].y : 0
                    PathPolyline { path: modelData.pts }
                }
            }
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            visible: root.layout !== null
            ShapePath {
                strokeColor: root.roadColor
                strokeWidth: 5
                fillColor: "transparent"
                PathAngleArc {
                    centerX: root.layout ? root.layout.ring.cx : 0
                    centerY: root.layout ? root.layout.ring.cy : 0
                    radiusX: root.layout ? root.layout.ring.r : 0
                    radiusY: root.layout ? root.layout.ring.r : 0
                    startAngle: 0
                    sweepAngle: 360
                }
            }
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            visible: root.layout !== null && root.layout.route.length > 1
            ShapePath {
                strokeColor: root.inkColor
                strokeWidth: root.routeWidth
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.RoundJoin
                startX: (root.layout && root.layout.route.length > 0) ? root.layout.route[0].x : 0
                startY: (root.layout && root.layout.route.length > 0) ? root.layout.route[0].y : 0
                PathPolyline { path: root.layout ? root.layout.route : [] }
            }
        }
    }

    // Open chevron at the end of the exit stub. Outside the clipped item so the
    // mitred point is never shaved.
    Canvas {
        id: arrowCanvas
        anchors.fill: parent
        antialiasing: true
        z: 10
        visible: root.hasMap && root.layout && root.layout.arrow

        Connections {
            target: root
            function onIsDarkChanged() { arrowCanvas.requestPaint() }
            function onLayoutChanged() { arrowCanvas.requestPaint() }
        }
        onVisibleChanged: if (visible) requestPaint()
        onAvailableChanged: if (available) requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            if (!root.layout || !root.layout.arrow) return
            var a = root.layout.arrow
            var aL = root.arrowLen, aW = root.arrowWidth
            var bx = a.x - aL * a.ux, by = a.y - aL * a.uy
            ctx.beginPath()
            ctx.moveTo(bx + aW * a.uy, by - aW * a.ux)
            ctx.lineTo(a.x, a.y)
            ctx.lineTo(bx - aW * a.uy, by + aW * a.ux)
            ctx.lineWidth = root.routeWidth
            ctx.lineCap = "butt"
            ctx.lineJoin = "miter"
            ctx.miterLimit = 10
            ctx.strokeStyle = root.inkColor
            ctx.stroke()
        }
    }

    // No ring anywhere in the data: fall back to the schematic rose.
    RoundaboutIcon {
        anchors.centerIn: parent
        visible: !root.hasMap
        exitNumber: typeof navigationService !== "undefined"
                    ? Math.max(1, navigationService.roundaboutExitCount) : 1
        isDark: root.isDark
        size: root.size
    }
}
