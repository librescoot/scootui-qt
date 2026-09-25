import QtQuick
import ScootUI 1.0

Item {
    id: root
    anchors.fill: parent
    z: 925

    property real milestoneKm: 0
    property string tag: ""
    property int intensity: 0
    property bool active: false

    readonly property int currentScreen: screenStore && screenStore.currentScreen !== undefined
                                         ? screenStore.currentScreen : 0
    readonly property bool allowedScreen: currentScreen === Scooter.ScreenMode.Cluster
                                       || currentScreen === Scooter.ScreenMode.Map

    readonly property var themeMap: ({
        "":         { a: "#D4AF37", b: "#F6E27A", c: "#D4AF37", paper: "#f6eedc", ink: "#8B6914", icon: "★", title: "Milestone reached" },
        "devil":    { a: "#7f0000", b: "#d32f2f", c: "#7f0000", paper: "#f7e9df", ink: "#8f1d20", icon: "☠", title: "666", subline: "A hell of a ride" },
        "nice69":   { a: "#8E24AA", b: "#E1BEE7", c: "#8E24AA", paper: "#f3eafa", ink: "#6A1B9A", icon: "♥", title: "696.9", subline: "nice" },
        "leet":     { a: "#00695C", b: "#00E676", c: "#00695C", paper: "#e7f5eb", ink: "#00695c", icon: "⚡", title: "L33T", subline: "Achievement unlocked" },
        "leet_rev": { a: "#004D40", b: "#64FFDA", c: "#004D40", paper: "#e5f3ef", ink: "#005747", icon: "⚡", title: "ELITE", subline: "Elite, even backwards" },
        "power2":   { a: "#0D47A1", b: "#64B5F6", c: "#0D47A1", paper: "#e9f0f7", ink: "#174887", icon: "◉", title: "2^10", subline: "A power of two on two wheels" },
        "sequence": { a: "#E91E63", b: "#FFEB3B", c: "#2196F3", paper: "#f8eaf0", ink: "#9b245e", icon: "♫", title: "1.2.3.4.5", subline: "Easy as pie" },
        "boobs":    { a: "#F48FB1", b: "#FFE0EC", c: "#F48FB1", paper: "#f9eaf1", ink: "#9b305f", icon: "♥", title: "nice", subline: "We saw that" },
        "rollover": { a: "#FF6F00", b: "#FFCA28", c: "#FF6F00", paper: "#f8eed9", ink: "#aa4500", icon: "⟳", title: "9999.9", subline: "Five digits, here we come" }
    })
    readonly property var theme: themeMap[tag] !== undefined ? themeMap[tag] : themeMap[""]
    readonly property var regularTaglines: [
        "The road is opening up", "Rolling right along", "Another stretch well traveled",
        "Your wheels know the way", "One more chapter on the road", "The journey keeps rolling",
        "Plenty more roads ahead", "That was worth the ride", "The long way looks good on you",
        "There's more around the bend", "Good roads make good stories", "Here's to the next hundred"
    ]
    readonly property string subline: {
        if (tag && theme.subline !== undefined) return theme.subline
        if (milestoneKm < 100) return "Getting your ride on"
        return regularTaglines[(Math.floor(milestoneKm / 100) - 1) % regularTaglines.length]
    }

    function formatKm(km) {
        return (Math.abs(km - Math.round(km)) < 0.05)
                ? Math.round(km) + " km"
                : km.toFixed(1) + " km"
    }

    function badgesForKm(km) {
        let units = Math.floor(km / 500)
        if (units < 1) return []
        const bronze = units % 5
        units = Math.floor(units / 5)
        const silver = units % 5
        units = Math.floor(units / 5)
        const gold = units % 5
        const diamond = Math.floor(units / 5)
        const tiers = []
        if (diamond) tiers.push({ symbol: "◆", count: diamond, color: "#15688E" })
        if (gold) tiers.push({ symbol: "★", count: gold, color: "#A9780C" })
        if (silver) tiers.push({ symbol: "★", count: silver, color: "#687786" })
        if (bronze) tiers.push({ symbol: "★", count: bronze, color: "#90532F" })
        return tiers
    }
    readonly property var badgeTiers: tag ? [] : badgesForKm(milestoneKm)

    Connections {
        target: odometerMilestoneService ? odometerMilestoneService : null
        function onMilestoneCelebrate(km, intens, tagIn) {
            if (!root.allowedScreen) {
                Qt.callLater(function() { odometerMilestoneService.advanceCelebration() })
                return
            }
            root.milestoneKm = km
            root.intensity = intens
            root.tag = tagIn
            root.active = true
            dismissTimer.interval = Math.max(3500, 1800 + intens * 450 + 1500)
            dismissTimer.restart()
        }
    }

    Timer {
        id: dismissTimer
        onTriggered: {
            root.active = false
            Qt.callLater(function() {
                if (typeof odometerMilestoneService !== "undefined")
                    odometerMilestoneService.advanceCelebration()
            })
        }
    }

    Item {
        id: ticket
        objectName: "milestoneTicket"
        width: Math.min(parent.width - 32, 414)
        height: 226
        anchors.centerIn: parent
        rotation: -2
        opacity: root.active ? 1 : 0
        scale: root.active ? 1 : 0.72
        visible: opacity > 0.01

        Rectangle {
            x: 0; y: 10
            width: parent.width; height: parent.height
            radius: 8
            color: "#99000000"
        }

        Rectangle {
            id: card
            anchors.fill: parent
            radius: 8
            clip: true
            color: root.theme.paper
            border.color: "#55777777"
            border.width: 1

            Rectangle {
                anchors.top: parent.top
                width: parent.width / 3; height: 8
                color: root.theme.a
            }
            Rectangle {
                anchors.top: parent.top
                x: parent.width / 3
                width: parent.width / 3; height: 8
                color: root.theme.b
            }
            Rectangle {
                anchors.top: parent.top
                x: parent.width * 2 / 3
                width: parent.width / 3; height: 8
                color: root.theme.c
            }

            Text {
                x: 7; y: 110
                text: root.theme.icon
                color: root.theme.ink
                opacity: 0.13
                font.pixelSize: 160
                rotation: -12
            }

            Text {
                x: 26; y: 27
                text: root.theme.title.toUpperCase()
                font.pixelSize: 13
                font.weight: Font.Bold
                font.letterSpacing: 1.5
                color: root.theme.ink
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 27
                y: 22
                visible: root.badgeTiers.length === 0
                text: root.theme.icon
                font.pixelSize: 26
                color: root.theme.ink
            }
            Row {
                id: badgeRow
                objectName: "milestoneBadges"
                anchors.right: parent.right
                anchors.rightMargin: 27
                y: 26
                spacing: 8
                readonly property var tiers: root.badgeTiers
                Repeater {
                    model: badgeRow.tiers
                    Row {
                        spacing: 1
                        Text {
                            text: modelData.symbol
                            color: modelData.color
                            font.pixelSize: 21
                            font.weight: Font.Bold
                        }
                        Text {
                            visible: modelData.count > 1
                            text: "×" + modelData.count
                            color: modelData.color
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
            Text {
                x: 26; y: 72
                width: parent.width - 52; height: 80
                text: root.formatKm(root.milestoneKm)
                font.family: "Roboto Condensed"
                font.pixelSize: 72
                font.weight: Font.Bold
                fontSizeMode: Text.Fit
                minimumPixelSize: 54
                color: "#202120"
            }
            Text {
                objectName: "milestoneSubline"
                x: 26; y: 151
                text: root.subline
                font.pixelSize: 15
                color: "#202120"
            }
            Repeater {
                model: Math.max(0, Math.floor((card.width - 52) / 9))
                Rectangle {
                    x: 26 + index * 9; y: 185
                    width: 5; height: 1
                    color: "#66777777"
                }
            }
            Text {
                x: 26; y: 199
                text: "MILESTONE"
                font.pixelSize: 12
                font.weight: Font.Bold
                font.letterSpacing: 1
                color: root.theme.ink
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 26
                y: 199
                text: root.formatKm(root.milestoneKm).toUpperCase()
                font.pixelSize: 12
                font.weight: Font.Bold
                font.letterSpacing: 1
                color: root.theme.ink
            }
        }

        Behavior on opacity { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
    }
}
