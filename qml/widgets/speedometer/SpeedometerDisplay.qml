import QtQuick
import "../indicators"

Item {
    id: speedometer

    // Properties from stores
    readonly property real targetSpeed: typeof engineStore !== "undefined" ? engineStore.speed : 0
    readonly property real motorCurrent: typeof engineStore !== "undefined" ? engineStore.motorCurrent : 0
    readonly property bool ecuStale: typeof engineStore !== "undefined" && engineStore.faultCode === 20
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

    // Speed labels to show (every 10 km/h for regulatory compliance)
    readonly property var speedLabels: [0, 10, 20, 30, 40, 50, 60]
    readonly property var majorSpeedLabels: [0, 30, 50, 60]

    // Fixed size matching Flutter (no scaling)
    readonly property real displayScale: 1.0

    // Imperative flag — avoids circular binding (animatedSpeed ↔ running)
    property bool _animationActive: false

    // Start animation when speed target changes
    onTargetSpeedChanged: _animationActive = true

    // Start animation when acceleration/pulse state changes
    onIsAcceleratingChanged: if (isAccelerating) _animationActive = true

    // Repaint on theme change
    onIsDarkChanged: canvas.requestPaint()

    // Exponential smoothing via FrameAnimation — only runs when animating
    FrameAnimation {
        id: frameAnim
        running: speedometer._animationActive
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

            // Auto-stop when fully converged and no pulse effects active
            var converged = (animatedSpeed === targetSpeed)
            var noPulse = animatedSpeed <= maxArcSpeed && !isAccelerating
            if (converged && noPulse) {
                speedometer._animationActive = false
            }
        }
    }

    // Regen transition animation
    Behavior on regenTransition {
        NumberAnimation { duration: 1000; easing.type: Easing.InOutQuad }
    }
    onIsRegeneratingChanged: regenTransition = isRegenerating ? 1.0 : 0.0
    onRegenTransitionChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -20
        width: canvasWidth * displayScale
        height: canvasHeight * displayScale

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
                var clampedSpeed = Math.min(animatedSpeed, maxArcSpeed)
                var progress = clampedSpeed / maxArcSpeed
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
            for (var speed = 0; speed <= maxArcSpeed; speed += 5) {
                var tickAngle = startRad + sweepRad * (speed / maxArcSpeed)
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
                ctx.strokeStyle = isDark ? "#80FFFFFF" : "#1F000000"
                ctx.stroke()
            }

            // === Speed labels ===
            var labelInward = 44
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            for (var i = 0; i < speedLabels.length; i++) {
                var spd = speedLabels[i]
                if (spd > maxArcSpeed) continue
                var isMajor = majorSpeedLabels.indexOf(spd) >= 0
                var labelAngle = startRad + sweepRad * (spd / maxArcSpeed)
                var labelR = r - labelInward
                var lx = cx + labelR * Math.cos(labelAngle)
                var ly = cy + labelR * Math.sin(labelAngle)
                ctx.font = isMajor ? "600 13px sans-serif" : "400 9px sans-serif"
                ctx.fillStyle = isMajor
                    ? (isDark ? "#CCFFFFFF" : "#4D000000")
                    : (isDark ? "#80FFFFFF" : "#1F000000")
                ctx.fillText(spd.toString(), lx, ly)
            }
        }
    }

    // Central speed, km/h and road info matching Flutter's Stack + Transform + Column
    // Speed number — anchored to arc center (centerY=150 in 240px canvas = parent.center + 30)
    Text {
        id: speedText
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height / 2 - height / 2
        text: speedometer.ecuStale ? "—" : Math.floor(speedometer.animatedSpeed).toString()
        font.pixelSize: themeStore.fontDisplay
        font.weight: Font.Bold
        color: speedometer.isDark ? "#FFFFFF" : "#000000"
    }

    // km/h — tight below speed number
    Text {
        id: unitText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: speedText.bottom
        anchors.topMargin: -12
        text: "km/h"
        font.pixelSize: themeStore.fontTitle
        color: speedometer.isDark ? "#99FFFFFF" : "#8A000000"
    }

    // Road name + speed limit — below km/h
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: unitText.bottom
        anchors.topMargin: 4
        spacing: 4

        SpeedLimitIndicator {
            iconSize: 27
            anchors.verticalCenter: parent.verticalCenter
        }

        RoadNameDisplay {
            anchors.verticalCenter: parent.verticalCenter
            fontSize: 12
        }
    }

    // Helper: linear interpolation between two colors
    function lerpColor(c1, c2, t) {
        // Parse #RRGGBB or #AARRGGBB
        var offset1 = c1.length > 7 ? 2 : 0
        var a1 = offset1 ? parseInt(c1.substring(1, 3), 16) / 255 : 1.0
        var r1 = parseInt(c1.substring(1 + offset1, 3 + offset1), 16)
        var g1 = parseInt(c1.substring(3 + offset1, 5 + offset1), 16)
        var b1 = parseInt(c1.substring(5 + offset1, 7 + offset1), 16)

        var offset2 = c2.length > 7 ? 2 : 0
        var a2 = offset2 ? parseInt(c2.substring(1, 3), 16) / 255 : 1.0
        var r2 = parseInt(c2.substring(1 + offset2, 3 + offset2), 16)
        var g2 = parseInt(c2.substring(3 + offset2, 5 + offset2), 16)
        var b2 = parseInt(c2.substring(5 + offset2, 7 + offset2), 16)

        var r = Math.round(r1 + (r2 - r1) * t)
        var g = Math.round(g1 + (g2 - g1) * t)
        var b = Math.round(b1 + (b2 - b1) * t)
        var a = a1 + (a2 - a1) * t

        return "rgba(" + r + "," + g + "," + b + "," + a.toFixed(3) + ")"
    }
}
