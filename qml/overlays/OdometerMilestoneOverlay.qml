import QtQuick
import ScootUI 1.0

// Gold/themed toast card in the bottom-right. The matching confetti layer
// (MilestoneConfettiLayer) is embedded inside Cluster/Map screens so
// confetti falls *behind* the widgets while this card floats on top.
Item {
    id: root
    anchors.fill: parent
    z: 920

    property real milestoneKm: 0
    property string tag: ""
    property bool active: false

    readonly property int currentScreen: ScreenStore && ScreenStore.currentScreen !== undefined
                                         ? ScreenStore.currentScreen : 0
    readonly property int screenModeCluster: 0
    readonly property int screenModeMap: 1
    readonly property bool allowedScreen: root.currentScreen === root.screenModeCluster
                                       || root.currentScreen === root.screenModeMap

    // Theme per tag.
    readonly property var themeMap: ({
        "":         { bg0: "#D4AF37", bg1: "#F6E27A", bg2: "#D4AF37", fg: "#1a1200", border: "#8B6914", icon: "\u2605", title: "Milestone" },
        "devil":    { bg0: "#7f0000", bg1: "#d32f2f", bg2: "#7f0000", fg: "#fff3b0", border: "#3a0000", icon: "\u2620", title: "666" },
        "leet":     { bg0: "#00695C", bg1: "#00E676", bg2: "#00695C", fg: "#002814", border: "#004D40", icon: "\u26A1", title: "L33T" },
        "leet_rev": { bg0: "#004D40", bg1: "#64FFDA", bg2: "#004D40", fg: "#002814", border: "#003830", icon: "\u26A1", title: "ELITE" },
        "power2":   { bg0: "#0D47A1", bg1: "#64B5F6", bg2: "#0D47A1", fg: "#E3F2FD", border: "#082C66", icon: "\u25C9", title: "2^10" },
        "sequence": { bg0: "#E91E63", bg1: "#FFEB3B", bg2: "#2196F3", fg: "#1a1200", border: "#6A1B9A", icon: "\u266B", title: "1.2.3.4.5" },
        "boobs":    { bg0: "#F48FB1", bg1: "#FFE0EC", bg2: "#F48FB1", fg: "#4A148C", border: "#AD1457", icon: "\u2665", title: "nice" },
        "rollover": { bg0: "#FF6F00", bg1: "#FFCA28", bg2: "#FF6F00", fg: "#3E2723", border: "#E65100", icon: "\u27F3", title: "9999.9" }
    })
    readonly property var theme: themeMap[tag] !== undefined ? themeMap[tag] : themeMap[""]

    function formatKm(km) {
        return (Math.abs(km - Math.round(km)) < 0.05)
                ? Math.round(km) + " km"
                : km.toFixed(1) + " km"
    }

    Connections {
        target: OdometerMilestoneService ? OdometerMilestoneService : null
        function onMilestoneReached(km, intens, tag) {
            if (!root.allowedScreen) {
                if (true)
                    OdometerMilestoneService.dismiss()
                return
            }
            root.milestoneKm = km
            root.tag = tag
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
            if (true)
                OdometerMilestoneService.dismiss()
        }
    }

    Rectangle {
        id: card
        width: contentRow.implicitWidth + 24
        height: contentRow.implicitHeight + 12
        radius: ThemeStore && ThemeStore.radiusCard !== undefined ? ThemeStore.radiusCard : 12
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 4
        anchors.bottomMargin: 4
        transformOrigin: Item.BottomRight
        opacity: root.active ? 1 : 0
        scale: root.active ? 1 : 0.7
        visible: opacity > 0.01

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.theme.bg0 }
            GradientStop { position: 0.5; color: root.theme.bg1 }
            GradientStop { position: 1.0; color: root.theme.bg2 }
        }

        border.color: root.theme.border
        border.width: 1

        Column {
            id: contentRow
            anchors.centerIn: parent
            spacing: 0
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.theme.icon
                    color: root.theme.fg
                    font.pixelSize: ThemeStore && ThemeStore.fontCaption !== undefined ? ThemeStore.fontCaption : 12
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.theme.title
                    color: root.theme.fg
                    font.pixelSize: ThemeStore && ThemeStore.fontCaption !== undefined ? ThemeStore.fontCaption : 12
                    font.weight: Font.Medium
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.formatKm(root.milestoneKm)
                color: root.theme.fg
                font.pixelSize: ThemeStore && ThemeStore.fontBody !== undefined ? ThemeStore.fontBody : 16
                font.weight: Font.Bold
            }
        }

        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on scale   { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }
    }
}
