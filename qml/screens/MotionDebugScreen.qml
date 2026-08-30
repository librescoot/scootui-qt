// BMX debug screen — developer view for aligning magnetic heading against
// GPS course and visualizing live IMU + drivetrain data. Triggered via:
//   redis-cli HSET settings dashboard.mode motion-debug
// Not exposed in any in-product menu.
//
// Layout on the 480×480 display:
//   ┌─────────────┬────────────────────────┐
//   │  horizon    │  gyro x/y/z over time  │   240 tall
//   │  200×240    │  280×240               │
//   ├─────────────┼────────────────────────┤
//   │  compass    │  mag heading 5 traces  │
//   │  200×240    │  280×120               │   240 tall
//   │             ├────────────────────────┤
//   │             │  speed (kph) + throttle│
//   │             │  280×120               │
//   └─────────────┴────────────────────────┘
//
// Heading colors used in both the compass needles and the strip-chart
// traces, so the eye can track the same series across both panels:
//   GPS course  red        #ff4040
//   raw         pale cyan  #7fc8ff
//   fast EMA    orange     #ffaa44
//   medium EMA  blue       #3aa0ff (the canonical heading_deg)
//   slow EMA    purple     #c060ff
import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Rectangle {
    id: screen
    color: "#0a0a0a"

    // Left hold leaves, same as every other full-screen page. The panels fill
    // all 480x480 so there is no room for a hint bar; without this the screen
    // could only be left by writing the mode key back by hand.
    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftHold() {
            if (typeof settingsService !== "undefined")
                settingsService.updateMode("speedometer")
            if (typeof navigator !== "undefined")
                navigator.setScreen(Scooter.ScreenMode.Cluster)
        }
    }

    // Sample buffers for the strip charts. Sized for ~30 s at 10 Hz.
    readonly property int bufLen: 300

    // Gyro 3-axis buffer (vehicle frame)
    property var gyroBuf: ({ x: new Array(bufLen).fill(0),
                             y: new Array(bufLen).fill(0),
                             z: new Array(bufLen).fill(0) })
    property int gyroIdx: 0

    // Mag heading buffer — five traces, all parallel through the same idx
    property var headBuf: ({ raw:  new Array(bufLen).fill(NaN),
                             fast: new Array(bufLen).fill(NaN),
                             med:  new Array(bufLen).fill(NaN),
                             slow: new Array(bufLen).fill(NaN),
                             gps:  new Array(bufLen).fill(NaN) })
    property int headIdx: 0

    // Engine speed/throttle buffer (sampled from engineStore at 10 Hz tick)
    property var engineBuf: ({ speed:    new Array(bufLen).fill(0),
                               throttle: new Array(bufLen).fill(0) })
    property int engineIdx: 0

    // Repaint pacing
    Timer {
        interval: 33
        running: true
        repeat: true
        onTriggered: {
            gyroChart.requestPaint()
            magChart.requestPaint()
            engineChart.requestPaint()
        }
    }

    // Sample engineStore at a steady 10 Hz so the chart has uniform time
    // steps (the store updates whenever the engine ECU pushes, which is
    // bursty).
    Timer {
        interval: 100
        running: true
        repeat: true
        onTriggered: {
            screen.engineBuf.speed[screen.engineIdx] = engineStore.speed
            screen.engineBuf.throttle[screen.engineIdx] = engineStore.throttle
            screen.engineIdx = (screen.engineIdx + 1) % screen.bufLen
        }
    }

    Connections {
        target: motionStore
        function onSensorsChanged() {
            screen.gyroBuf.x[screen.gyroIdx] = motionStore.gyroX
            screen.gyroBuf.y[screen.gyroIdx] = motionStore.gyroY
            screen.gyroBuf.z[screen.gyroIdx] = motionStore.gyroZ
            screen.gyroIdx = (screen.gyroIdx + 1) % screen.bufLen
            horizon.requestPaint()
        }
        function onHeadingChanged() {
            screen.headBuf.raw[screen.headIdx]  = motionStore.headingRawDeg
            screen.headBuf.fast[screen.headIdx] = motionStore.headingFastDeg
            screen.headBuf.med[screen.headIdx]  = motionStore.headingDeg
            screen.headBuf.slow[screen.headIdx] = motionStore.headingSlowDeg
            screen.headBuf.gps[screen.headIdx]  =
                (gpsStore && gpsStore.hasValidGps) ? gpsStore.course : NaN
            screen.headIdx = (screen.headIdx + 1) % screen.bufLen
            compass.requestPaint()
        }
    }

    GridLayout {
        anchors.fill: parent
        columns: 2
        rows: 2
        rowSpacing: 0
        columnSpacing: 0

        // ─── Top-left: virtual horizon ──────────────────────────────
        Rectangle {
            Layout.preferredWidth: 200
            Layout.preferredHeight: 240
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

                    // Vehicle-frame NED accel: at rest level (ax,ay,az)=(0,0,-g).
                    // Use level-aware tilt formulas so 0 = level (instead of
                    // the standard NED Tait-Bryan which gives 180° at rest).
                    const ax = motionStore.accelX
                    const ay = motionStore.accelY
                    const az = motionStore.accelZ
                    const roll = Math.atan2(ay, -az)     // + = right lean
                    const pitch = Math.atan2(ax, -az)    // + = nose up

                    const pitchPxPerRad = 90 / (Math.PI / 2)
                    const horizonY = cy + pitch * pitchPxPerRad

                    ctx.save()
                    ctx.translate(cx, cy)
                    ctx.rotate(-roll)
                    ctx.translate(-cx, -cy)

                    ctx.fillStyle = "#1a3a6a"
                    ctx.fillRect(-width, -height, width * 3, horizonY + height)
                    ctx.fillStyle = "#5a3a1a"
                    ctx.fillRect(-width, horizonY, width * 3, height * 3)
                    ctx.strokeStyle = "#fff"; ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.moveTo(-width, horizonY); ctx.lineTo(width * 2, horizonY)
                    ctx.stroke()

                    // Pitch ladder ±60°
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

                    // Fixed center reticle
                    ctx.strokeStyle = "#ffaa44"; ctx.lineWidth = 3
                    ctx.beginPath()
                    ctx.moveTo(cx - 30, cy); ctx.lineTo(cx - 8, cy)
                    ctx.moveTo(cx + 8, cy); ctx.lineTo(cx + 30, cy)
                    ctx.moveTo(cx, cy - 4); ctx.lineTo(cx, cy + 4)
                    ctx.stroke()

                    // Roll arc
                    const arcR = Math.min(cx, cy) - 14
                    ctx.strokeStyle = "#444"
                    ctx.beginPath()
                    ctx.arc(cx, cy, arcR, -Math.PI * 0.75, -Math.PI * 0.25); ctx.stroke()
                    const ang = -Math.PI / 2 - roll
                    ctx.fillStyle = "#ffaa44"
                    ctx.beginPath()
                    ctx.moveTo(cx + arcR * Math.cos(ang), cy + arcR * Math.sin(ang))
                    ctx.lineTo(cx + (arcR - 8) * Math.cos(ang) - 5,
                               cy + (arcR - 8) * Math.sin(ang))
                    ctx.lineTo(cx + (arcR - 8) * Math.cos(ang) + 5,
                               cy + (arcR - 8) * Math.sin(ang))
                    ctx.closePath(); ctx.fill()

                    ctx.fillStyle = "#fff"
                    ctx.font = "11px monospace"
                    ctx.textAlign = "left"; ctx.textBaseline = "top"
                    ctx.fillText("R " + (roll * 180 / Math.PI).toFixed(1) + "°", 4, 4)
                    ctx.textAlign = "right"
                    ctx.fillText("P " + (pitch * 180 / Math.PI).toFixed(1) + "°", width - 4, 4)
                    ctx.textAlign = "left"; ctx.textBaseline = "bottom"
                    ctx.fillText("|a|" + motionStore.accelMagnitude.toFixed(2) + "g", 4, height - 4)
                    ctx.textAlign = "right"
                    ctx.fillText(motionStore.tiltCompensated ? "tilt-comp" : "X/Y only",
                                 width - 4, height - 4)
                }
            }
        }

        // ─── Top-right: gyro 3-axis strip chart ─────────────────────
        Rectangle {
            Layout.preferredWidth: 280
            Layout.preferredHeight: 240
            color: "#000"
            border.color: "#222"; border.width: 1

            Canvas {
                id: gyroChart
                anchors.fill: parent
                property real maxRange: 30
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    let peak = 5
                    for (let i = 0; i < screen.bufLen; ++i) {
                        peak = Math.max(peak,
                            Math.abs(screen.gyroBuf.x[i]),
                            Math.abs(screen.gyroBuf.y[i]),
                            Math.abs(screen.gyroBuf.z[i]))
                    }
                    maxRange = maxRange * 0.9 + Math.max(10, peak * 1.2) * 0.1

                    // Gridlines
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
                            const idx = (screen.gyroIdx + i) % screen.bufLen
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

                    ctx.fillStyle = "#888"
                    ctx.font = "10px monospace"
                    ctx.textAlign = "left"; ctx.textBaseline = "top"
                    ctx.fillText("gyro ±" + maxRange.toFixed(0) + "°/s  (vehicle frame)", 4, 2)
                    ctx.textAlign = "right"
                    ctx.fillStyle = "#ff5050"; ctx.fillText("X (roll)", width - 110, 2)
                    ctx.fillStyle = "#50ff50"; ctx.fillText("Y (pitch)", width - 56, 2)
                    ctx.fillStyle = "#5080ff"; ctx.fillText("Z (yaw)", width - 6, 2)

                    // Live numerics top-right corner
                    ctx.font = "11px monospace"
                    ctx.fillStyle = "#ddd"
                    ctx.textAlign = "right"; ctx.textBaseline = "bottom"
                    ctx.fillText("X " + motionStore.gyroX.toFixed(1) + "°/s", width - 4, height - 30)
                    ctx.fillText("Y " + motionStore.gyroY.toFixed(1) + "°/s", width - 4, height - 16)
                    ctx.fillText("Z " + motionStore.gyroZ.toFixed(1) + "°/s", width - 4, height - 2)
                }
            }
        }

        // ─── Bottom-left: compass ───────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 200
            Layout.preferredHeight: 240
            color: "#0a0a0a"
            border.color: "#222"; border.width: 1

            Canvas {
                id: compass
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const cx = width / 2
                    const cy = height / 2 + 4
                    const r = Math.min(cx, cy) - 14

                    ctx.strokeStyle = "#444"
                    ctx.lineWidth = 1
                    ctx.beginPath(); ctx.arc(cx, cy, r, 0, 2 * Math.PI); ctx.stroke()
                    ctx.fillStyle = "#888"
                    ctx.font = "10px sans-serif"
                    ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    for (let a = 0; a < 360; a += 10) {
                        const major = (a % 30 === 0)
                        const len = major ? 8 : 3
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
                            const tx = cx + (r - 18) * Math.cos(rad)
                            const ty = cy + (r - 18) * Math.sin(rad)
                            ctx.fillStyle = (a === 0) ? "#ff5050" : "#888"
                            ctx.fillText(label, tx, ty)
                        }
                    }

                    function needle(deg, color, length, lw, dash) {
                        if (deg === undefined || isNaN(deg)) return
                        const rad = (deg - 90) * Math.PI / 180
                        ctx.strokeStyle = color
                        ctx.lineWidth = lw
                        ctx.setLineDash(dash || [])
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.lineTo(cx + length * Math.cos(rad), cy + length * Math.sin(rad))
                        ctx.stroke()
                        ctx.setLineDash([])
                        ctx.fillStyle = color
                        ctx.beginPath()
                        ctx.arc(cx + length * Math.cos(rad), cy + length * Math.sin(rad),
                                3, 0, 2 * Math.PI)
                        ctx.fill()
                    }

                    // GPS course (red, dashed) — only shown with a fix
                    if (gpsStore && gpsStore.hasValidGps)
                        needle(gpsStore.course, "#ff4040", r - 2, 2, [4, 3])

                    // Magnetic heading needles, longest = freshest, all same length
                    // category but slightly stepped to avoid total overlap.
                    needle(motionStore.headingSlowDeg, "#c060ff", r - 8,  2)
                    needle(motionStore.headingDeg,     "#3aa0ff", r - 14, 2)
                    needle(motionStore.headingFastDeg, "#ffaa44", r - 20, 2)
                    needle(motionStore.headingRawDeg,  "#7fc8ff", r - 26, 2)

                    ctx.fillStyle = "#bbb"
                    ctx.beginPath(); ctx.arc(cx, cy, 3, 0, 2 * Math.PI); ctx.fill()

                    // Numeric corner readouts
                    ctx.font = "10px monospace"
                    ctx.textAlign = "left"; ctx.textBaseline = "top"
                    if (gpsStore && gpsStore.hasValidGps) {
                        ctx.fillStyle = "#ff4040"
                        ctx.fillText("GPS  " + gpsStore.course.toFixed(0) + "°", 4, 4)
                    }
                    ctx.fillStyle = "#7fc8ff"; ctx.fillText("raw  " + motionStore.headingRawDeg.toFixed(0) + "°", 4, 18)
                    ctx.fillStyle = "#ffaa44"; ctx.fillText("fast " + motionStore.headingFastDeg.toFixed(0) + "°", 4, 30)
                    ctx.fillStyle = "#3aa0ff"; ctx.fillText("med  " + motionStore.headingDeg.toFixed(0) + "°", 4, 42)
                    ctx.fillStyle = "#c060ff"; ctx.fillText("slow " + motionStore.headingSlowDeg.toFixed(0) + "°", 4, 54)

                    ctx.textAlign = "right"
                    ctx.fillStyle = "#999"
                    ctx.fillText("±" + motionStore.accuracyDeg.toFixed(1) + "°", width - 4, 4)
                    ctx.fillText("|B|" + motionStore.magStrengthUT.toFixed(1), width - 4, 16)
                    if (gpsStore && gpsStore.hasValidGps) {
                        let d = motionStore.headingDeg - gpsStore.course
                        d = ((d + 540) % 360) - 180
                        ctx.fillStyle = "#dddd66"
                        ctx.fillText("Δ " + (d >= 0 ? "+" : "") + d.toFixed(0) + "°",
                                     width - 4, 28)
                    }
                }
            }
        }

        // ─── Bottom-right: stacked mag-heading + engine charts ──────
        ColumnLayout {
            Layout.preferredWidth: 280
            Layout.preferredHeight: 240
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                color: "#000"
                border.color: "#222"; border.width: 1

                Canvas {
                    id: magChart
                    anchors.fill: parent
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

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
                                const idx = (screen.headIdx + i) % screen.bufLen
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
                        // Drawn back-to-front so live ones are on top.
                        plot(screen.headBuf.gps,  "#ff4040", 1)
                        plot(screen.headBuf.raw,  "#7fc8ff", 1)
                        plot(screen.headBuf.slow, "#c060ff", 1)
                        plot(screen.headBuf.fast, "#ffaa44", 1)
                        plot(screen.headBuf.med,  "#3aa0ff", 1)

                        ctx.fillStyle = "#888"
                        ctx.font = "10px monospace"
                        ctx.textAlign = "left"; ctx.textBaseline = "top"
                        ctx.fillText("heading 0–360°", 4, 2)
                        ctx.textAlign = "right"
                        ctx.fillStyle = "#ff4040"; ctx.fillText("gps",  width - 124, 2)
                        ctx.fillStyle = "#7fc8ff"; ctx.fillText("raw",  width - 96, 2)
                        ctx.fillStyle = "#ffaa44"; ctx.fillText("fast", width - 68, 2)
                        ctx.fillStyle = "#3aa0ff"; ctx.fillText("med",  width - 36, 2)
                        ctx.fillStyle = "#c060ff"; ctx.fillText("slow", width - 4,  2)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                color: "#000"
                border.color: "#222"; border.width: 1

                Canvas {
                    id: engineChart
                    anchors.fill: parent
                    readonly property real maxKph: 60
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        // Speed axis gridlines (0/15/30/45/60 kph)
                        ctx.strokeStyle = "#222"; ctx.lineWidth = 1
                        for (let kph = 0; kph <= 60; kph += 15) {
                            const y = height - (kph / 60) * (height - 14) - 14
                            ctx.beginPath()
                            ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                        }

                        // Throttle band (top 10 px): filled where throttle is
                        // engaged. engineStore.throttle is a bool, true =
                        // engaged.
                        ctx.fillStyle = "#553300"
                        for (let i = 0; i < screen.bufLen; ++i) {
                            const idx = (screen.engineIdx + i) % screen.bufLen
                            if (screen.engineBuf.throttle[idx]) {
                                const x = (i / (screen.bufLen - 1)) * width
                                ctx.fillRect(x, 0, Math.ceil(width / screen.bufLen) + 1, 10)
                            }
                        }

                        // Speed line
                        ctx.strokeStyle = "#66dd66"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        let started = false
                        for (let i = 0; i < screen.bufLen; ++i) {
                            const idx = (screen.engineIdx + i) % screen.bufLen
                            const v = screen.engineBuf.speed[idx]
                            const x = (i / (screen.bufLen - 1)) * width
                            const clamped = Math.max(0, Math.min(60, v))
                            const y = height - (clamped / 60) * (height - 14) - 14
                            if (!started) { ctx.moveTo(x, y); started = true }
                            else ctx.lineTo(x, y)
                        }
                        ctx.stroke()

                        // Labels
                        ctx.fillStyle = "#888"
                        ctx.font = "10px monospace"
                        ctx.textAlign = "left"; ctx.textBaseline = "top"
                        ctx.fillText("speed 0–60 kph", 4, 2)
                        ctx.textAlign = "right"
                        ctx.fillStyle = "#66dd66"
                        ctx.fillText(engineStore.speed.toFixed(0) + " kph", width - 4, 2)

                        ctx.fillStyle = "#888"
                        ctx.textBaseline = "middle"; ctx.textAlign = "right"
                        for (let kph = 0; kph <= 60; kph += 30) {
                            const y = height - (kph / 60) * (height - 14) - 14
                            ctx.fillText(kph.toString(), width - 4, y)
                        }

                        // Throttle indicator
                        ctx.fillStyle = engineStore.throttle ? "#ff8800" : "#553300"
                        ctx.fillRect(4, 12, 12, 6)
                        ctx.fillStyle = "#888"
                        ctx.textAlign = "left"; ctx.textBaseline = "top"
                        ctx.fillText("throttle " + (engineStore.throttle ? "on" : "off"),
                                     20, 11)
                    }
                }
            }
        }
    }
}
