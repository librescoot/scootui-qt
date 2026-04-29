// BMX debug screen — developer view for aligning magnetic heading against
// GPS course and visualizing live IMU data. Triggered via:
//   redis-cli HSET settings mode bmx-debug
// Not exposed in any in-product menu. Three panels on a 480×480 display:
//
//   ┌──────────────────────────────────────┐
//   │      Compass (220 px tall)           │   GPS course vs mag heading,
//   │                                      │   raw + smoothed needles
//   ├─────────────┬────────────────────────┤
//   │  Horizon    │  Strip charts          │   accel/gyro-derived attitude;
//   │  120 sq     │  ¬gyro xyz over time   │   gyro & mag heading scrolling
//   │             │  ¬mag heading over time│   left-to-right, raw-data dense
//   ├─────────────┴────────────────────────┤
//   │  Numeric readout strip               │   live values, single-decimal
//   └──────────────────────────────────────┘
import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: screen
    color: "#0a0a0a"

    // Sample buffers used for the strip charts. Pre-sized to roughly the
    // longest visible window (300 samples ≈ 30 s at 10 Hz). New samples
    // overwrite the oldest in a circular fashion; the chart Canvas walks
    // them in chronological order on each repaint.
    readonly property int bufLen: 300
    property var gyroBuf: ({ x: new Array(bufLen).fill(0),
                             y: new Array(bufLen).fill(0),
                             z: new Array(bufLen).fill(0) })
    property var magBuf:  ({ raw: new Array(bufLen).fill(NaN),
                             smooth: new Array(bufLen).fill(NaN) })
    property int writeIdx: 0
    property int writeIdxMag: 0

    // Repaint pacing — strip charts redraw at 30 Hz independently of the
    // sensor update rate so panning looks smooth even at 10 Hz data.
    Timer {
        interval: 33
        running: true
        repeat: true
        onTriggered: { gyroChart.requestPaint(); magChart.requestPaint() }
    }

    // Pull every gyro/accel sample.
    Connections {
        target: bmxStore
        function onSensorsChanged() {
            screen.gyroBuf.x[screen.writeIdx] = bmxStore.gyroX
            screen.gyroBuf.y[screen.writeIdx] = bmxStore.gyroY
            screen.gyroBuf.z[screen.writeIdx] = bmxStore.gyroZ
            screen.writeIdx = (screen.writeIdx + 1) % screen.bufLen
        }
        function onHeadingChanged() {
            screen.magBuf.raw[screen.writeIdxMag] = bmxStore.headingRawDeg
            screen.magBuf.smooth[screen.writeIdxMag] = bmxStore.headingDeg
            screen.writeIdxMag = (screen.writeIdxMag + 1) % screen.bufLen
            compass.requestPaint()
            horizon.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ─── Compass ────────────────────────────────────────────────
        Rectangle {
            id: compassPanel
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            color: "#0a0a0a"
            border.color: "#222"
            border.width: 1

            Canvas {
                id: compass
                anchors.fill: parent
                renderStrategy: Canvas.Cooperative
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const cx = width / 2
                    const cy = height / 2 + 4
                    const r = Math.min(cx, cy) - 14

                    // Outer ring + 30° tick marks
                    ctx.strokeStyle = "#444"
                    ctx.lineWidth = 1
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2 * Math.PI); ctx.stroke()
                    ctx.fillStyle = "#888"
                    ctx.font = "12px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    for (let a = 0; a < 360; a += 10) {
                        const major = (a % 30 === 0)
                        const len = major ? 10 : 4
                        const rad = (a - 90) * Math.PI / 180
                        const x1 = cx + (r - len) * Math.cos(rad)
                        const y1 = cy + (r - len) * Math.sin(rad)
                        const x2 = cx + r * Math.cos(rad)
                        const y2 = cy + r * Math.sin(rad)
                        ctx.strokeStyle = major ? "#888" : "#444"
                        ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke()
                        if (major) {
                            const labels = { 0: "N", 90: "E", 180: "S", 270: "W" }
                            const label = labels[a] || a
                            const tx = cx + (r - 22) * Math.cos(rad)
                            const ty = cy + (r - 22) * Math.sin(rad)
                            ctx.fillStyle = (a === 0) ? "#ff5050" : "#888"
                            ctx.fillText(label, tx, ty)
                        }
                    }

                    function needle(deg, color, length, width, dash) {
                        if (deg === undefined || isNaN(deg)) return
                        const rad = (deg - 90) * Math.PI / 180
                        ctx.strokeStyle = color
                        ctx.lineWidth = width
                        ctx.setLineDash(dash || [])
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.lineTo(cx + length * Math.cos(rad), cy + length * Math.sin(rad))
                        ctx.stroke()
                        ctx.setLineDash([])
                        // Small filled head
                        const headRad = 4
                        ctx.fillStyle = color
                        ctx.beginPath()
                        ctx.arc(cx + length * Math.cos(rad), cy + length * Math.sin(rad),
                                headRad, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    // GPS course (red, solid). Hidden when no fix.
                    if (gpsStore && gpsStore.hasValidGps)
                        needle(gpsStore.course, "#ff4040", r - 2, 3)

                    // Magnetic heading: smoothed (cyan, dashed) and raw (cyan
                    // bright, solid, slightly shorter so they don't overlap).
                    needle(bmxStore.headingDeg, "#3aa0ff", r - 8, 2, [4, 3])
                    needle(bmxStore.headingRawDeg, "#7fc8ff", r - 28, 3)

                    // Center pip
                    ctx.fillStyle = "#bbb"
                    ctx.beginPath(); ctx.arc(cx, cy, 3, 0, 2 * Math.PI); ctx.fill()
                }
            }

            // Numeric overlay top-left
            Column {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 2
                Text {
                    color: "#ff8080"
                    font.family: "monospace"; font.pixelSize: 14
                    text: gpsStore && gpsStore.hasValidGps
                          ? "GPS  " + gpsStore.course.toFixed(1) + "°"
                          : "GPS  no fix"
                }
                Text {
                    color: "#7fc8ff"
                    font.family: "monospace"; font.pixelSize: 14
                    text: "MAG  " + bmxStore.headingRawDeg.toFixed(1) + "°"
                }
                Text {
                    color: "#3aa0ff"
                    font.family: "monospace"; font.pixelSize: 14
                    text: "smth " + bmxStore.headingDeg.toFixed(1) + "°"
                }
                Text {
                    color: bmxStore.tiltCompensated ? "#88dd88" : "#dd8888"
                    font.family: "monospace"; font.pixelSize: 11
                    text: "±" + bmxStore.accuracyDeg.toFixed(1) + "° "
                          + (bmxStore.tiltCompensated ? "tilt-comp" : "X/Y only")
                }
            }

            // Numeric overlay top-right
            Column {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 2
                Text {
                    color: "#999"; font.family: "monospace"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight; width: 130
                    text: "|B|  " + bmxStore.magStrengthUT.toFixed(1) + " µT"
                }
                Text {
                    color: "#999"; font.family: "monospace"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight; width: 130
                    text: "tilt " + bmxStore.tiltDeg.toFixed(1) + "°"
                }
                Text {
                    color: "#999"; font.family: "monospace"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight; width: 130
                    text: "|ω|  " + bmxStore.yawRateDPS.toFixed(1) + " °/s"
                }
                Text {
                    color: gpsStore && gpsStore.hasValidGps && bmxStore.headingRawDeg > 0
                           ? "#dddd66" : "#666"
                    font.family: "monospace"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight; width: 130
                    text: {
                        if (!(gpsStore && gpsStore.hasValidGps)) return "Δ    --"
                        let d = bmxStore.headingDeg - gpsStore.course
                        // Wrap to (-180, 180]
                        d = ((d + 540) % 360) - 180
                        return "Δ    " + (d >= 0 ? "+" : "") + d.toFixed(1) + "°"
                    }
                }
            }
        }

        // ─── Horizon + strip charts ─────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: "#000"
                border.color: "#222"; border.width: 1

                Canvas {
                    id: horizon
                    anchors.fill: parent
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        const cx = width / 2
                        const cy = height / 2

                        // Roll/pitch from accelerometer (radians).
                        const ax = bmxStore.accelX
                        const ay = bmxStore.accelY
                        const az = bmxStore.accelZ
                        const roll = Math.atan2(ay, az)
                        const pitch = Math.atan2(-ax, Math.sqrt(ay*ay + az*az))

                        const pitchPxPerRad = 90 / (Math.PI / 2)  // ±90° fills the panel
                        const horizonY = cy + pitch * pitchPxPerRad

                        ctx.save()
                        ctx.translate(cx, cy)
                        ctx.rotate(-roll)
                        ctx.translate(-cx, -cy)

                        // Sky
                        ctx.fillStyle = "#1a3a6a"
                        ctx.fillRect(-width, -height, width * 3, horizonY + height)
                        // Ground
                        ctx.fillStyle = "#5a3a1a"
                        ctx.fillRect(-width, horizonY, width * 3, height * 3)
                        // Horizon line
                        ctx.strokeStyle = "#fff"; ctx.lineWidth = 2
                        ctx.beginPath()
                        ctx.moveTo(-width, horizonY); ctx.lineTo(width * 2, horizonY)
                        ctx.stroke()

                        // Pitch ladder ±60° at 15° intervals
                        ctx.strokeStyle = "#ddd"; ctx.lineWidth = 1
                        ctx.fillStyle = "#ddd"
                        ctx.font = "10px monospace"
                        ctx.textAlign = "right"; ctx.textBaseline = "middle"
                        for (let p = -60; p <= 60; p += 15) {
                            if (p === 0) continue
                            const y = cy + (pitch - p * Math.PI / 180) * pitchPxPerRad
                            const w = (Math.abs(p) % 30 === 0) ? 30 : 15
                            ctx.beginPath()
                            ctx.moveTo(cx - w, y); ctx.lineTo(cx + w, y); ctx.stroke()
                            if (Math.abs(p) % 30 === 0)
                                ctx.fillText(p.toString(), cx - w - 4, y)
                        }
                        ctx.restore()

                        // Fixed center reticle (orange aircraft symbol)
                        ctx.strokeStyle = "#ffaa44"; ctx.lineWidth = 3
                        ctx.beginPath()
                        ctx.moveTo(cx - 30, cy); ctx.lineTo(cx - 8, cy)
                        ctx.moveTo(cx + 8, cy); ctx.lineTo(cx + 30, cy)
                        ctx.moveTo(cx, cy - 4); ctx.lineTo(cx, cy + 4)
                        ctx.stroke()

                        // Roll arc & marker at top
                        const arcR = Math.min(cx, cy) - 14
                        ctx.strokeStyle = "#444"
                        ctx.beginPath()
                        ctx.arc(cx, cy, arcR, -Math.PI * 0.75, -Math.PI * 0.25); ctx.stroke()
                        // Roll marker
                        const ang = -Math.PI / 2 - roll
                        ctx.fillStyle = "#ffaa44"
                        ctx.beginPath()
                        ctx.moveTo(cx + arcR * Math.cos(ang), cy + arcR * Math.sin(ang))
                        ctx.lineTo(cx + (arcR - 8) * Math.cos(ang) - 5,
                                   cy + (arcR - 8) * Math.sin(ang))
                        ctx.lineTo(cx + (arcR - 8) * Math.cos(ang) + 5,
                                   cy + (arcR - 8) * Math.sin(ang))
                        ctx.closePath(); ctx.fill()

                        // Numeric corners
                        ctx.fillStyle = "#fff"
                        ctx.font = "11px monospace"
                        ctx.textAlign = "left"; ctx.textBaseline = "top"
                        ctx.fillText("R " + (roll * 180 / Math.PI).toFixed(1) + "°", 4, 4)
                        ctx.textAlign = "right"
                        ctx.fillText("P " + (pitch * 180 / Math.PI).toFixed(1) + "°", width - 4, 4)
                        ctx.textAlign = "left"; ctx.textBaseline = "bottom"
                        ctx.fillText("|a|" + bmxStore.accelMagnitude.toFixed(2) + "g", 4, height - 4)
                    }
                }
            }

            // Strip charts: top half is gyro xyz, bottom half is mag heading.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#000"
                border.color: "#222"; border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Canvas {
                        id: gyroChart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        property real maxRange: 30  // °/s, autoscaled
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            // Compute autoscale from current buffer
                            let peak = 5
                            for (let i = 0; i < screen.bufLen; ++i) {
                                peak = Math.max(peak,
                                    Math.abs(screen.gyroBuf.x[i]),
                                    Math.abs(screen.gyroBuf.y[i]),
                                    Math.abs(screen.gyroBuf.z[i]))
                            }
                            // Smooth the autoscale so ticks don't jump every frame
                            maxRange = maxRange * 0.9 + Math.max(10, peak * 1.2) * 0.1

                            // Gridlines (zero, ±half, ±full)
                            ctx.strokeStyle = "#222"; ctx.lineWidth = 1
                            for (const f of [-1, -0.5, 0, 0.5, 1]) {
                                const y = height / 2 - f * (height / 2 - 4)
                                ctx.beginPath()
                                ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                            }

                            function plot(buf, color) {
                                ctx.strokeStyle = color
                                ctx.lineWidth = 1
                                ctx.beginPath()
                                let started = false
                                for (let i = 0; i < screen.bufLen; ++i) {
                                    const idx = (screen.writeIdx + i) % screen.bufLen
                                    const v = buf[idx]
                                    if (isNaN(v)) { started = false; continue }
                                    const x = (i / (screen.bufLen - 1)) * width
                                    const y = height / 2 - (v / maxRange) * (height / 2 - 4)
                                    if (!started) { ctx.moveTo(x, y); started = true }
                                    else ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                            }
                            plot(screen.gyroBuf.x, "#ff5050")
                            plot(screen.gyroBuf.y, "#50ff50")
                            plot(screen.gyroBuf.z, "#5080ff")

                            // Scale label & legend
                            ctx.fillStyle = "#888"
                            ctx.font = "10px monospace"
                            ctx.textAlign = "left"; ctx.textBaseline = "top"
                            ctx.fillText("gyro ±" + maxRange.toFixed(0) + "°/s", 4, 2)
                            ctx.textAlign = "right"
                            ctx.fillStyle = "#ff5050"; ctx.fillText("X", width - 26, 2)
                            ctx.fillStyle = "#50ff50"; ctx.fillText("Y", width - 16, 2)
                            ctx.fillStyle = "#5080ff"; ctx.fillText("Z", width - 6, 2)
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#222" }

                    Canvas {
                        id: magChart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            // Heading wraps; we draw the value directly across
                            // 0–360 and break the path on big jumps to avoid
                            // a vertical line spanning the panel at the wrap.
                            ctx.strokeStyle = "#222"; ctx.lineWidth = 1
                            for (const f of [0, 90, 180, 270]) {
                                const y = height - (f / 360) * (height - 2) - 1
                                ctx.beginPath()
                                ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                            }

                            function plot(buf, color, lw) {
                                ctx.strokeStyle = color
                                ctx.lineWidth = lw
                                ctx.beginPath()
                                let prev = NaN
                                for (let i = 0; i < screen.bufLen; ++i) {
                                    const idx = (screen.writeIdxMag + i) % screen.bufLen
                                    const v = buf[idx]
                                    if (isNaN(v)) { prev = NaN; continue }
                                    const x = (i / (screen.bufLen - 1)) * width
                                    const y = height - (v / 360) * (height - 2) - 1
                                    if (isNaN(prev) || Math.abs(v - prev) > 180) {
                                        ctx.moveTo(x, y)
                                    } else {
                                        ctx.lineTo(x, y)
                                    }
                                    prev = v
                                }
                                ctx.stroke()
                            }
                            plot(screen.magBuf.raw, "#7fc8ff", 1)
                            plot(screen.magBuf.smooth, "#3aa0ff", 1)

                            ctx.fillStyle = "#888"
                            ctx.font = "10px monospace"
                            ctx.textAlign = "left"; ctx.textBaseline = "top"
                            ctx.fillText("mag heading 0–360°", 4, 2)
                            ctx.textAlign = "right"
                            ctx.fillStyle = "#7fc8ff"; ctx.fillText("raw", width - 32, 2)
                            ctx.fillStyle = "#3aa0ff"; ctx.fillText("smth", width - 4, 2)
                        }
                    }
                }
            }
        }

        // ─── Numeric strip ──────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0a0a"
            border.color: "#222"; border.width: 1

            GridLayout {
                anchors.fill: parent
                anchors.margins: 8
                columns: 4
                rowSpacing: 2
                columnSpacing: 16

                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "ax" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.accelX.toFixed(3) + " g" }
                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "mx" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.magX.toFixed(2) + " µT" }

                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "ay" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.accelY.toFixed(3) + " g" }
                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "my" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.magY.toFixed(2) + " µT" }

                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "az" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.accelZ.toFixed(3) + " g" }
                Text { color:"#888"; font.family:"monospace"; font.pixelSize: 11; text: "mz" }
                Text { color:"#fff"; font.family:"monospace"; font.pixelSize: 12
                       text: bmxStore.magZ.toFixed(2) + " µT" }
            }
        }
    }
}
