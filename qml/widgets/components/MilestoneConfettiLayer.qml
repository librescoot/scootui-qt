import QtQuick
import QtQuick.Particles
import ScootUI 1.0

// Drop this into a screen at a low z value so confetti falls behind widgets.
// Listens to OdometerMilestoneService.milestoneReached and bursts accordingly.
Item {
    id: root
    anchors.fill: parent

    property int intensity: 0
    property bool emitting: false
    property string tag: ""

    readonly property int emitDurationMs: Math.round(1800 + intensity * 450)
    readonly property int edgeRate: Math.round(35 + intensity * 18)
    readonly property int cardRate: Math.round(20 + intensity * 10)

    // Confetti colours vary by tag so easter eggs get their own palette.
    // Each palette is intentionally small (3 colours) to keep the confetti
    // readable and on-theme with the card. Light-theme variants drop any
    // near-white colour so particles stay visible on a light background.
    readonly property bool dark: ThemeStore.isDark

    readonly property var paletteStandard: dark
        ? ["#D4AF37", "#F6E27A", "#FFFFFF"]
        : ["#8B6914", "#D4AF37", "#F6E27A"]
    readonly property var paletteDevil: dark
        ? ["#E53935", "#212121", "#FFEB3B"]
        : ["#B71C1C", "#4A0000", "#E65100"]
    readonly property var paletteLeet: dark
        ? ["#00E676", "#64FFDA", "#FFFFFF"]
        : ["#00695C", "#00B8D4", "#2E7D32"]
    readonly property var palettePower: dark
        ? ["#29B6F6", "#80DEEA", "#FFFFFF"]
        : ["#0D47A1", "#1976D2", "#6A1B9A"]
    readonly property var paletteSequence: dark
        ? ["#E91E63", "#FFEB3B", "#2196F3"]
        : ["#C2185B", "#F57F17", "#1565C0"]
    readonly property var paletteBoobs: dark
        ? ["#F48FB1", "#FFE0EC", "#FFFFFF"]
        : ["#AD1457", "#EC407A", "#4A148C"]
    readonly property var paletteRollover: dark
        ? ["#FF6F00", "#FFCA28", "#FFFFFF"]
        : ["#E65100", "#BF360C", "#FF6F00"]

    readonly property var activePalette: {
        switch (tag) {
            case "devil":    return paletteDevil
            case "leet":
            case "leet_rev": return paletteLeet
            case "power2":   return palettePower
            case "sequence": return paletteSequence
            case "boobs":    return paletteBoobs
            case "rollover": return paletteRollover
            default:         return paletteStandard
        }
    }

    Connections {
        target: OdometerMilestoneService ? OdometerMilestoneService : null
        function onMilestoneReached(km, intens, tag) {
            root.tag = tag
            root.intensity = intens
            sys.running = true
            root.emitting = true
            stopTimer.interval = root.emitDurationMs
            stopTimer.restart()
            // Keep system running long enough for the longest-lived particle
            // (~3800 ms after emitter stops) to finish falling.
            systemStopTimer.interval = root.emitDurationMs + 4000
            systemStopTimer.restart()
        }
    }

    Timer {
        id: stopTimer
        onTriggered: root.emitting = false
    }

    // Keep the system running through the full particle lifespan after
    // emission stops so in-flight confetti can finish falling.
    Timer {
        id: systemStopTimer
        onTriggered: sys.running = false
    }

    ParticleSystem {
        id: sys
        running: false
    }

    Component {
        id: piece
        Rectangle {
            width: 5 + Math.random() * 6
            height: 2 + Math.random() * 3
            radius: 1
            readonly property var pal: root.activePalette
            color: pal[Math.floor(Math.random() * pal.length)]
            opacity: 0.95
            NumberAnimation on rotation {
                from: 0; to: 720
                duration: 2500 + Math.random() * 1500
                loops: Animation.Infinite
            }
        }
    }

    ItemParticle {
        system: sys
        delegate: piece
        fade: true
    }

    Emitter {
        id: emitterLeft
        system: sys
        x: 0
        y: root.height * 0.35
        width: 1
        height: root.height * 0.4
        emitRate: root.emitting ? root.edgeRate : 0
        lifeSpan: 3000
        lifeSpanVariation: 800
        size: 1
        velocity: AngleDirection {
            angle: -25; angleVariation: 30
            magnitude: 280; magnitudeVariation: 100
        }
        acceleration: PointDirection { y: 280; yVariation: 60 }
    }

    Emitter {
        id: emitterRight
        system: sys
        x: root.width - 1
        y: root.height * 0.35
        width: 1
        height: root.height * 0.4
        emitRate: root.emitting ? root.edgeRate : 0
        lifeSpan: 3000
        lifeSpanVariation: 800
        size: 1
        velocity: AngleDirection {
            angle: 205; angleVariation: 30
            magnitude: 280; magnitudeVariation: 100
        }
        acceleration: PointDirection { y: 280; yVariation: 60 }
    }

    // Upward burst from the bottom-right (where the toast card sits)
    Emitter {
        system: sys
        x: root.width - 60
        y: root.height - 58
        width: 80
        height: 20
        emitRate: root.emitting ? root.cardRate : 0
        lifeSpan: 2500
        lifeSpanVariation: 600
        size: 1
        velocity: AngleDirection {
            angle: 270; angleVariation: 140
            magnitude: 260; magnitudeVariation: 100
        }
        acceleration: PointDirection { y: 340; yVariation: 60 }
    }
}
