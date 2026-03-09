import QtQuick

Item {
    id: speedometer

    // Properties from stores
    readonly property real targetSpeed: typeof engineStore !== "undefined" ? engineStore.speed : 0
    readonly property real motorCurrent: typeof engineStore !== "undefined" ? engineStore.motorCurrent : 0
    readonly property bool isDark: themeStore.isDark

    // Internal animated speed
    property real animatedSpeed: 0
    property real maxArcSpeed: 60

    // Animation state
    property bool isRegenerating: motorCurrent < 0
    property bool isAccelerating: motorCurrent > 0
    property real regenTransition: 0
    property real overspeedPulse: 0
    property real accelPulse: 0

    // Arc constants
    readonly property real arcStartAngle: 150
    readonly property real arcSweepAngle: 240
    readonly property real arcStrokeWidth: 20
    readonly property real canvasWidth: 300
    readonly property real canvasHeight: 240
    readonly property real centerX: canvasWidth / 2
    readonly property real centerY: 150
    readonly property real arcRadius: canvasWidth / 2

    // Speed labels to show
    readonly property var speedLabels: [0, 30, 50, 60, 70, 80, 90, 100, 120]

    // Scale to fit container
    readonly property real scaleX: width / canvasWidth
    readonly property real scaleY: height / (canvasHeight + 40) // extra space for labels
    readonly property real displayScale: Math.min(scaleX, scaleY)

    // Exponential smoothing via FrameAnimation
    FrameAnimation {
        id: frameAnim
        running: true
        onTriggered: {
            var dtMs = frameTime * 1000
            if (dtMs <= 0 || dtMs > 500) dtMs = 16

            // Exponential smoothing: alpha = 1 - exp(-dt/100)
            var alpha = 1.0 - Math.exp(-dtMs / 100.0)
            var diff = targetSpeed - animatedSpeed
            if (Math.abs(diff) < 0.3) {
                animatedSpeed = targetSpeed
            } else {
                animatedSpeed += diff * alpha
            }

            // Overspeed pulse (800ms cycle)
            if (animatedSpeed > maxArcSpeed) {
                overspeedPulse = (Math.sin(Date.now() / 800 * Math.PI * 2) + 1) / 2
            } else {
                overspeedPulse = 0
            }

            // Acceleration pulse (1000ms cycle)
            if (isAccelerating) {
                accelPulse = (Math.sin(Date.now() / 1000 * Math.PI * 2) + 1) / 2
            } else {
                accelPulse = 0
            }

            canvas.requestPaint()
        }
    }

    // Regen transition animation
    Behavior on regenTransition {
        NumberAnimation { duration: 1000; easing.type: Easing.InOutQuad }
    }
    onIsRegeneratingChanged: regenTransition = isRegenerating ? 1.0 : 0.0

    Canvas {
        id: canvas
        anchors.centerIn: parent
        width: canvasWidth * displayScale
        height: (canvasHeight + 40) * displayScale

        property real s: displayScale

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.scale(s, s)

            var cx = centerX
            var cy = centerY
            var r = arcRadius

            var startRad = arcStartAngle * Math.PI / 180
            var sweepRad = arcSweepAngle * Math.PI / 180

            // === Background arc ===
            ctx.beginPath()
            ctx.arc(cx, cy, r - arcStrokeWidth / 2, startRad, startRad + sweepRad)
            ctx.lineWidth = arcStrokeWidth
            ctx.lineCap = "round"

            // Regen color: grey → red transition
            if (regenTransition > 0) {
                var bgR = lerpColor(isDark ? "#424242" : "#E0E0E0", "#4DFF0000", regenTransition)
                ctx.strokeStyle = bgR
            } else {
                ctx.strokeStyle = isDark ? "#424242" : "#E0E0E0"
            }
            ctx.stroke()

            // === Speed fill arc ===
            if (animatedSpeed > 0) {
                var clampedSpeed = Math.min(animatedSpeed, 120)
                var progress = clampedSpeed / 120
                var fillSweep = sweepRad * progress

                ctx.beginPath()
                ctx.arc(cx, cy, r - arcStrokeWidth / 2, startRad, startRad + fillSweep)
                ctx.lineWidth = arcStrokeWidth
                ctx.lineCap = "round"

                // Color based on speed
                var fillColor
                if (animatedSpeed > maxArcSpeed) {
                    // Overspeed: pulse purple ↔ pink
                    fillColor = lerpColor("#9C27B0", "#E91E63", overspeedPulse)
                } else if (animatedSpeed > 55) {
                    // Transition zone 55-60: blue → purple
                    var t = (animatedSpeed - 55) / 5
                    fillColor = lerpColor("#2196F3", "#9C27B0", t)
                } else {
                    fillColor = "#2196F3"
                }

                // Acceleration pulse modifies opacity
                if (isAccelerating && animatedSpeed <= maxArcSpeed) {
                    var opacity = 0.7 + 0.3 * accelPulse
                    ctx.globalAlpha = opacity
                }

                ctx.strokeStyle = fillColor
                ctx.stroke()
                ctx.globalAlpha = 1.0
            }

            // === Tick marks ===
            var tickInward = 26
            for (var speed = 0; speed <= 120; speed += 5) {
                var tickAngle = startRad + sweepRad * (speed / 120)
                var isMajor = (speed % 10 === 0)
                var tickLen = isMajor ? 8 : 4
                var tickWidth = isMajor ? 1.5 : 1.0

                var outerR = r - tickInward
                var innerR = outerR - tickLen

                var cosA = Math.cos(tickAngle)
                var sinA = Math.sin(tickAngle)

                ctx.beginPath()
                ctx.moveTo(cx + outerR * cosA, cy + outerR * sinA)
                ctx.lineTo(cx + innerR * cosA, cy + innerR * sinA)
                ctx.lineWidth = tickWidth
                ctx.lineCap = "butt"
                ctx.strokeStyle = isDark ? "#4DFFFFFF" : "#1F000000"
                ctx.stroke()
            }

            // === Speed labels ===
            var labelInward = 44
            ctx.font = "500 11px sans-serif"
            ctx.fillStyle = isDark ? "#4DFFFFFF" : "#1F000000"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            for (var i = 0; i < speedLabels.length; i++) {
                var spd = speedLabels[i]
                if (spd > 120) continue
                var labelAngle = startRad + sweepRad * (spd / 120)
                var labelR = r - labelInward
                var lx = cx + labelR * Math.cos(labelAngle)
                var ly = cy + labelR * Math.sin(labelAngle)
                ctx.fillText(spd.toString(), lx, ly)
            }

            // === Central speed text ===
            ctx.font = "bold 96px sans-serif"
            ctx.fillStyle = isDark ? "#FFFFFF" : "#000000"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.fillText(Math.floor(animatedSpeed).toString(), cx, cy + 40)

            // km/h label
            ctx.font = "22px sans-serif"
            ctx.fillStyle = isDark ? "#99FFFFFF" : "#8A000000"
            ctx.fillText("km/h", cx, cy + 80)
        }
    }

    // Helper: linear interpolation between two colors
    function lerpColor(c1, c2, t) {
        // Parse hex colors
        var r1 = parseInt(c1.substring(1, 3), 16) || parseInt(c1.substring(3, 5), 16)
        var g1 = parseInt(c1.substring(3, 5), 16) || parseInt(c1.substring(5, 7), 16)
        var b1 = parseInt(c1.substring(5, 7), 16) || parseInt(c1.substring(7, 9), 16)

        // Handle colors that might have alpha prefix (#AARRGGBB)
        var offset1 = c1.length > 7 ? 2 : 0
        r1 = parseInt(c1.substring(1 + offset1, 3 + offset1), 16)
        g1 = parseInt(c1.substring(3 + offset1, 5 + offset1), 16)
        b1 = parseInt(c1.substring(5 + offset1, 7 + offset1), 16)

        var offset2 = c2.length > 7 ? 2 : 0
        var r2 = parseInt(c2.substring(1 + offset2, 3 + offset2), 16)
        var g2 = parseInt(c2.substring(3 + offset2, 5 + offset2), 16)
        var b2 = parseInt(c2.substring(5 + offset2, 7 + offset2), 16)

        var r = Math.round(r1 + (r2 - r1) * t)
        var g = Math.round(g1 + (g2 - g1) * t)
        var b = Math.round(b1 + (b2 - b1) * t)

        return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1)
    }
}
