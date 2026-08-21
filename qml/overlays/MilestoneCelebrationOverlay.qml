import QtQuick
import ScootUI 1.0

// Large centered banner shown when the scooter parks with one or more
// queued milestone crossings from the ride. Companion to the confetti
// layer (MilestoneConfettiLayer). Sequential: when the hold ends, asks
// the service to advance to the next queued milestone (if any).
Item {
    id: root
    anchors.fill: parent
    z: 925

    property real milestoneKm: 0
    property string tag: ""
    property int intensity: 0
    property bool active: false

    readonly property int currentScreen: ScreenStore && ScreenStore.currentScreen !== undefined
                                         ? ScreenStore.currentScreen : 0
    readonly property bool allowedScreen: currentScreen === Scooter.ScreenMode.Cluster
                                       || currentScreen === Scooter.ScreenMode.Map

    readonly property var themeMap: ({
        "":         { bg0: "#D4AF37", bg1: "#F6E27A", bg2: "#D4AF37", fg: "#1a1200", border: "#8B6914", icon: "★", title: "Milestone reached" },
        "devil":    { bg0: "#7f0000", bg1: "#d32f2f", bg2: "#7f0000", fg: "#fff3b0", border: "#3a0000", icon: "☠", title: "666" },
        "leet":     { bg0: "#00695C", bg1: "#00E676", bg2: "#00695C", fg: "#002814", border: "#004D40", icon: "⚡", title: "L33T" },
        "leet_rev": { bg0: "#004D40", bg1: "#64FFDA", bg2: "#004D40", fg: "#002814", border: "#003830", icon: "⚡", title: "ELITE" },
        "power2":   { bg0: "#0D47A1", bg1: "#64B5F6", bg2: "#0D47A1", fg: "#E3F2FD", border: "#082C66", icon: "◉", title: "2^10" },
        "sequence": { bg0: "#E91E63", bg1: "#FFEB3B", bg2: "#2196F3", fg: "#1a1200", border: "#6A1B9A", icon: "♫", title: "1.2.3.4.5" },
        "boobs":    { bg0: "#F48FB1", bg1: "#FFE0EC", bg2: "#F48FB1", fg: "#4A148C", border: "#AD1457", icon: "♥", title: "nice" },
        "rollover": { bg0: "#FF6F00", bg1: "#FFCA28", bg2: "#FF6F00", fg: "#3E2723", border: "#E65100", icon: "⟳", title: "9999.9" }
    })
    readonly property var theme: themeMap[tag] !== undefined ? themeMap[tag] : themeMap[""]

    function formatKm(km) {
        return (Math.abs(km - Math.round(km)) < 0.05)
                ? Math.round(km) + " km"
                : km.toFixed(1) + " km"
    }

    Connections {
        target: OdometerMilestoneService ? OdometerMilestoneService : null
        function onMilestoneCelebrate(km, intens, tagIn) {
            if (!root.allowedScreen) {
                // Skip to next queued item — overlay only renders on
                // cluster/map.
                Qt.callLater(function() {
                    OdometerMilestoneService.advanceCelebration()
                })
                return
            }
            root.milestoneKm = km
            root.intensity = intens
            root.tag = tagIn
            root.active = true
            var holdMs = Math.max(3500, 1800 + intens * 450 + 1500)
            dismissTimer.interval = holdMs
            dismissTimer.restart()
        }
    }

    Timer {
        id: dismissTimer
        onTriggered: {
            root.active = false
            // Give the exit animation a beat before the next banner pops in.
            Qt.callLater(function() {
                if (typeof OdometerMilestoneService !== "undefined")
                    OdometerMilestoneService.advanceCelebration()
            })
        }
    }

    Rectangle {
        id: card
        width: Math.min(parent.width - 32, contentCol.implicitWidth + 60)
        height: contentCol.implicitHeight + 36
        radius: ThemeStore && ThemeStore.radiusCard !== undefined ? ThemeStore.radiusCard * 1.5 : 18
        anchors.centerIn: parent
        transformOrigin: Item.Center
        opacity: root.active ? 1 : 0
        scale: root.active ? 1 : 0.6
        visible: opacity > 0.01

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.theme.bg0 }
            GradientStop { position: 0.5; color: root.theme.bg1 }
            GradientStop { position: 1.0; color: root.theme.bg2 }
        }

        border.color: root.theme.border
        border.width: 2

        Column {
            id: contentCol
            anchors.centerIn: parent
            spacing: 6

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.theme.icon
                color: root.theme.fg
                font.pixelSize: 56
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.theme.title
                color: root.theme.fg
                font.pixelSize: ThemeStore && ThemeStore.fontTitle !== undefined ? ThemeStore.fontTitle : 22
                font.weight: Font.Medium
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.formatKm(root.milestoneKm)
                color: root.theme.fg
                font.pixelSize: 72
                font.weight: Font.Bold
            }
        }

        Behavior on opacity { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
        Behavior on scale   { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
    }
}
