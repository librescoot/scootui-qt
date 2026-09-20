import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property bool active: false
    property bool dark: true
    property real speedExcess: 0
    property real arcStartAngle: 150
    property real arcSweepAngle: 240
    property real arcRadius: 150
    property real centerX: width / 2
    property real centerY: height / 2
    property real elapsed: 0
    property bool stopping: false
    property real stopElapsed: 0

    readonly property var streakColors: dark
        ? ["#F6FBFF", "#BBDEFB", "#E1BEE7", "#B2DFDB"]
        : ["#1565C0", "#7B1FA2", "#00897B", "#EF6C00"]
    readonly property int baseStreakCount: 64
    readonly property int streakCount: baseStreakCount
                                      + Math.min(56, Math.floor(Math.max(0, speedExcess - 1) * 3))

    opacity: active || stopping ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 450; easing.type: Easing.InOutQuad }
    }

    onActiveChanged: {
        if (active) {
            stopping = false
            stopTimer.stop()
        } else if (opacity > 0 || stopping) {
            stopElapsed = elapsed
            stopping = true
            stopTimer.restart()
        }
    }

    FrameAnimation {
        running: root.active || root.stopping
        onTriggered: root.elapsed += frameTime
        onRunningChanged: if (!running) root.elapsed = 0
    }

    Timer {
        id: stopTimer
        interval: 650
        onTriggered: root.stopping = false
    }

    function random(index, salt) {
        const value = Math.sin((index + 1) * 12.9898 + salt * 78.233) * 43758.5453
        return value - Math.floor(value)
    }

    Repeater {
        model: root.streakCount

        Shape {
            id: streak
            required property int index

            readonly property real angle: root.arcStartAngle
                                          + root.arcSweepAngle * root.random(index, 1)
            readonly property real launchOffset: root.random(index, 2)
            readonly property real travelRate: 1.8 + root.random(index, 3) * 2.7
            readonly property real activeProgress: (root.elapsed * travelRate + launchOffset) % 1
            readonly property real stopProgress: (root.stopElapsed * travelRate + launchOffset) % 1
            readonly property real progress: root.stopping
                                          ? Math.min(1, stopProgress
                                                    + (root.elapsed - root.stopElapsed) * travelRate)
                                          : activeProgress
            readonly property real colorSeed: root.random(index, 7)
            readonly property color streakColor: colorSeed > 0.93
                                                 ? (root.dark ? "#FFCC80" : "#E65100")
                                                 : colorSeed > 0.86
                                                   ? (root.dark ? "#80DEEA" : "#006064")
                                                   : root.streakColors[Math.floor(colorSeed
                                                                                  * root.streakColors.length)]
            readonly property real length: 8 + root.random(index, 4) * 92 + progress * 24
            readonly property real innerWidth: 1.1 + root.random(index, 5) * 1.4
            readonly property real outerWidth: innerWidth + 1.1 + root.random(index, 6) * 1.8
            readonly property real radius: root.arcRadius + 8 + progress * 450

            width: outerWidth
            height: length
            opacity: root.stopping && progress >= 1 ? 0 : 0.3 + progress * 0.7
            rotation: angle - 90
            x: root.centerX + (radius + length / 2) * Math.cos(angle * Math.PI / 180) - width / 2
            y: root.centerY + (radius + length / 2) * Math.sin(angle * Math.PI / 180) - height / 2
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: streak.streakColor
                strokeWidth: 0
                PathMove { x: (streak.width - streak.innerWidth) / 2; y: 0 }
                PathLine { x: (streak.width + streak.innerWidth) / 2; y: 0 }
                PathLine { x: streak.width; y: streak.height }
                PathLine { x: 0; y: streak.height }
                PathLine { x: (streak.width - streak.innerWidth) / 2; y: 0 }
            }
        }
    }
}
