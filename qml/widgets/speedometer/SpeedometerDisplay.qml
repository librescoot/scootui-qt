import QtQuick
import QtQuick.Shapes
import "../indicators"

Item {
    id: speedometer

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] SpeedometerDisplay completed")

    // Properties from stores
    readonly property real targetSpeed: {
        if (typeof settingsStore !== "undefined" && typeof engineStore !== "undefined") {
            if (settingsStore.showRawSpeed && engineStore.hasRawSpeed)
                return engineStore.rawSpeed
        }
        return typeof engineStore !== "undefined" ? engineStore.speed : 0
    }
    readonly property real motorCurrent: typeof engineStore !== "undefined" ? engineStore.motorCurrent : 0
    readonly property bool ecuStale: typeof engineStore !== "undefined" && engineStore.faultCode === 20
    readonly property bool isDark: themeStore.isDark

    // Internal animated speed
    property real animatedSpeed: 0
    property real maxArcSpeed: 60

    // Animation state
    property bool isRegenerating: motorCurrent < 0
    property bool isAccelerating: motorCurrent > 0
    property real overspeedPulse: 0
    property real accelPulse: 0
    property real errorPulse: 0

    // Regen fade: grey track bleeds to red over a second and back
    property real regenTransition: isRegenerating ? 1.0 : 0.0
    Behavior on regenTransition {
        NumberAnimation { duration: 1000; easing.type: Easing.InOutQuad }
    }

    // Arc constants
    readonly property real arcStartAngle: 150
    readonly property real arcSweepAngle: 240
    readonly property real arcStrokeWidth: 20
    readonly property real canvasWidth: 300
    readonly property real canvasHeight: 240
    readonly property real centerX: canvasWidth / 2
    readonly property real centerY: 150
    readonly property real arcRadius: canvasWidth / 2
    readonly property real arcMidRadius: arcRadius - arcStrokeWidth / 2

    // Speed labels to show (every 10 km/h for regulatory compliance)
    readonly property var speedLabels: [0, 10, 20, 30, 40, 50, 60]
    readonly property var majorSpeedLabels: [0, 30, 50, 60]

    // Tick geometry
    readonly property real tickInward: 26
    readonly property real labelInward: 44

    // Fixed size matching Flutter (no scaling)
    readonly property real displayScale: 1.0

    // Palette
    readonly property color trackBaseColor: isDark ? "#424242" : "#E0E0E0"
    readonly property color regenTintColor: "#4DFF0000"
    readonly property color dimColor: isDark ? "#80FFFFFF" : "#1F000000"
    readonly property color majorLabelColor: isDark ? "#CCFFFFFF" : "#4D000000"
    readonly property color errorColor: "#F44336"
    // lerpColor reads .r/.g/.b/.a, so these have to be color properties rather
    // than string literals passed straight into the call.
    readonly property color lowSpeedColor: "#90CAF9"     // Material Blue 200
    readonly property color normalSpeedColor: "#2196F3"  // Material Blue 500
    readonly property color overspeedLowColor: "#9C27B0"
    readonly property color overspeedHighColor: "#E91E63"

    // Speed at which the blue reaches full intensity, and where the ramp
    // towards purple starts.
    readonly property real fullIntensitySpeed: 55

    readonly property color trackColor: regenTransition > 0
        ? lerpColor(trackBaseColor, regenTintColor, regenTransition)
        : trackBaseColor

    readonly property color speedFillColor: {
        if (animatedSpeed > maxArcSpeed)
            return lerpColor(overspeedLowColor, overspeedHighColor, overspeedPulse)
        if (animatedSpeed > fullIntensitySpeed)
            return lerpColor(normalSpeedColor, overspeedLowColor,
                             (animatedSpeed - fullIntensitySpeed) / (maxArcSpeed - fullIntensitySpeed))
        return lerpColor(lowSpeedColor, normalSpeedColor, animatedSpeed / fullIntensitySpeed)
    }

    readonly property real fillSweep:
        arcSweepAngle * Math.min(animatedSpeed, maxArcSpeed) / maxArcSpeed

    // Imperative flag, avoids a circular binding (animatedSpeed <-> running)
    property bool _animationActive: false

    onTargetSpeedChanged: _animationActive = true

    // Exponential smoothing towards targetSpeed. Stops as soon as it converges;
    // the pulses below are declarative, so nothing has to keep this alive to
    // animate them.
    FrameAnimation {
        running: speedometer._animationActive
        onTriggered: {
            var dtMs = frameTime * 1000
            if (dtMs <= 0 || dtMs > 500) dtMs = 16

            var alpha = 1.0 - Math.exp(-dtMs / 100.0)
            var diff = speedometer.targetSpeed - speedometer.animatedSpeed
            if (Math.abs(diff) < 0.3) {
                speedometer.animatedSpeed = speedometer.targetSpeed
                speedometer._animationActive = false
            } else {
                speedometer.animatedSpeed += diff * alpha
            }
        }
    }

    // Overspeed: purple <-> pink, 800 ms cycle
    SequentialAnimation {
        running: speedometer.animatedSpeed > speedometer.maxArcSpeed
        loops: Animation.Infinite
        onRunningChanged: if (!running) speedometer.overspeedPulse = 0
        NumberAnimation {
            target: speedometer; property: "overspeedPulse"
            from: 0; to: 1; duration: 400; easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: speedometer; property: "overspeedPulse"
            from: 1; to: 0; duration: 400; easing.type: Easing.InOutSine
        }
    }

    // Acceleration: fill arc breathes, 1000 ms cycle
    SequentialAnimation {
        running: speedometer.isAccelerating && speedometer.animatedSpeed <= speedometer.maxArcSpeed
        loops: Animation.Infinite
        onRunningChanged: if (!running) speedometer.accelPulse = 0
        NumberAnimation {
            target: speedometer; property: "accelPulse"
            from: 0; to: 1; duration: 500; easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: speedometer; property: "accelPulse"
            from: 1; to: 0; duration: 500; easing.type: Easing.InOutSine
        }
    }

    // ECU comm-lost (E20): the whole arc glows red, 1000 ms cycle
    SequentialAnimation {
        running: speedometer.ecuStale
        loops: Animation.Infinite
        onRunningChanged: if (!running) speedometer.errorPulse = 0
        NumberAnimation {
            target: speedometer; property: "errorPulse"
            from: 0; to: 1; duration: 500; easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: speedometer; property: "errorPulse"
            from: 1; to: 0; duration: 500; easing.type: Easing.InOutSine
        }
    }

    // The dial. Each arc is its own Shape so that animating the speed fill does
    // not re-tessellate the static track, and the markings are plain scene-graph
    // items that batch instead of being re-rasterised every frame.
    //
    // CurveRenderer, not GeometryRenderer: the geometry renderer only gets
    // smooth edges from window multisampling, which we do not enable, so the
    // arcs come out visibly stair-stepped. CurveRenderer antialiases in the
    // fragment shader instead, which is what Canvas used to give us via
    // QPainter.
    Item {
        id: dial
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -20
        width: speedometer.canvasWidth
        height: speedometer.canvasHeight
        scale: speedometer.displayScale

        // Background arc
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: speedometer.trackColor
                strokeWidth: speedometer.arcStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: speedometer.centerX
                    centerY: speedometer.centerY
                    radiusX: speedometer.arcMidRadius
                    radiusY: speedometer.arcMidRadius
                    startAngle: speedometer.arcStartAngle
                    sweepAngle: speedometer.arcSweepAngle
                }
            }
        }

        // ECU comm-lost glow: a wide translucent stroke under a solid one,
        // standing in for the Canvas shadowBlur this replaced.
        Shape {
            anchors.fill: parent
            visible: speedometer.ecuStale
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Qt.rgba(0.957, 0.263, 0.212, 0.15 + 0.35 * speedometer.errorPulse)
                strokeWidth: speedometer.arcStrokeWidth + 12
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: speedometer.centerX
                    centerY: speedometer.centerY
                    radiusX: speedometer.arcMidRadius
                    radiusY: speedometer.arcMidRadius
                    startAngle: speedometer.arcStartAngle
                    sweepAngle: speedometer.arcSweepAngle
                }
            }
            ShapePath {
                strokeColor: speedometer.errorColor
                strokeWidth: speedometer.arcStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: speedometer.centerX
                    centerY: speedometer.centerY
                    radiusX: speedometer.arcMidRadius
                    radiusY: speedometer.arcMidRadius
                    startAngle: speedometer.arcStartAngle
                    sweepAngle: speedometer.arcSweepAngle
                }
            }
        }

        // Speed fill arc
        Shape {
            anchors.fill: parent
            visible: speedometer.animatedSpeed > 0 && !speedometer.ecuStale
            opacity: speedometer.isAccelerating && speedometer.animatedSpeed <= speedometer.maxArcSpeed
                     ? 0.7 + 0.3 * speedometer.accelPulse : 1.0
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: speedometer.speedFillColor
                strokeWidth: speedometer.arcStrokeWidth
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: speedometer.centerX
                    centerY: speedometer.centerY
                    radiusX: speedometer.arcMidRadius
                    radiusY: speedometer.arcMidRadius
                    startAngle: speedometer.arcStartAngle
                    sweepAngle: speedometer.fillSweep
                }
            }
        }

        // Tick marks, every 5 km/h
        Repeater {
            model: Math.floor(speedometer.maxArcSpeed / 5) + 1

            Rectangle {
                required property int index

                readonly property real tickSpeed: index * 5
                readonly property bool isMajor: tickSpeed % 10 === 0
                readonly property real angle: speedometer.arcStartAngle
                                              + speedometer.arcSweepAngle * tickSpeed / speedometer.maxArcSpeed
                readonly property real outerR: speedometer.arcRadius - speedometer.tickInward
                readonly property real midR: outerR - height / 2

                width: isMajor ? 1.5 : 1.0
                height: isMajor ? 8 : 4
                color: speedometer.dimColor
                antialiasing: true

                x: speedometer.centerX + midR * Math.cos(angle * Math.PI / 180) - width / 2
                y: speedometer.centerY + midR * Math.sin(angle * Math.PI / 180) - height / 2
                rotation: angle - 90
            }
        }

        // Speed labels
        Repeater {
            model: speedometer.speedLabels

            Text {
                required property int index
                required property var modelData

                readonly property bool isMajor: speedometer.majorSpeedLabels.indexOf(modelData) >= 0
                readonly property real angle: speedometer.arcStartAngle
                                              + speedometer.arcSweepAngle * modelData / speedometer.maxArcSpeed
                readonly property real labelR: speedometer.arcRadius - speedometer.labelInward

                text: modelData.toString()
                font.family: "Roboto"
                font.pixelSize: isMajor ? 13 : 9
                font.weight: isMajor ? Font.DemiBold : Font.Normal
                color: isMajor ? speedometer.majorLabelColor : speedometer.dimColor

                x: speedometer.centerX + labelR * Math.cos(angle * Math.PI / 180) - width / 2
                y: speedometer.centerY + labelR * Math.sin(angle * Math.PI / 180) - height / 2
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

    // Road name + speed limit — below km/h. Sits lower in the gap between the
    // speed-arc endpoints, with a larger limit sign and room for a wider name.
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: unitText.bottom
        anchors.topMargin: 12
        spacing: 4

        SpeedLimitIndicator {
            iconSize: 36
            anchors.verticalCenter: parent.verticalCenter
        }

        RoadNameDisplay {
            anchors.verticalCenter: parent.verticalCenter
            fontSize: 12
            maxTextWidth: 240
        }
    }

    // Helper: linear interpolation between two colors, alpha included.
    // Takes color objects rather than hex strings: the operands are now color
    // properties (so the bindings re-evaluate on a theme flip), and this keeps
    // two parseInt-per-component hex parses out of a binding that re-runs on
    // every frame the speed changes.
    function lerpColor(c1, c2, t) {
        return Qt.rgba(c1.r + (c2.r - c1.r) * t,
                       c1.g + (c2.g - c1.g) * t,
                       c1.b + (c2.b - c1.b) * t,
                       c1.a + (c2.a - c1.a) * t)
    }
}
