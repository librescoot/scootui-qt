import QtQuick

// Full-screen overlay for the hop-on combo learning flow.
// Shows a live row of chips, one per captured token, growing left-to-right
// as the user presses brakes / horn / blinker switch. A countdown ticks
// from 5000 ms down; the buffer is committed when it reaches 0 (provided
// the user pressed at least 2 inputs).
Item {
    id: learnOverlay
    anchors.fill: parent

    // HopOnStore.Mode.Learning == 1
    readonly property int modeLearning: 1

    property int mode: typeof hopOnStore !== "undefined" ? hopOnStore.mode : 0
    property var tokens: typeof hopOnStore !== "undefined" ? hopOnStore.capturedTokens : []
    property int idleMs: typeof hopOnStore !== "undefined" ? hopOnStore.idleMillisRemaining : 0
    // Experimental gate: never render unless `experimental.hop-on` is set.
    property bool gateOpen: typeof settingsStore !== "undefined" && settingsStore.experimentalHopOn

    visible: gateOpen && mode === modeLearning

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color scrimColor:    isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor:     isDark ? "#CC000000" : "#CCFFFFFF"
    readonly property color cardBorder:    isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary:   isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"
    readonly property color chipColor:     isDark ? "#1FFFFFFF" : "#1F000000"
    readonly property color accentColor:   "#FF9800"

    // Short labels for the chips. Kept terse so chips stay small and
    // a long sequence still fits the card width via the wrapping Flow.
    function labelFor(token) {
        if (token === "LB") return "L"
        if (token === "RB") return "R"
        if (token === "HORN") return "Hrn"
        if (token === "BL") return "B\u2190"
        if (token === "BR") return "B\u2192"
        if (token === "SBOX") return "Box"
        return token
    }

    Rectangle {
        anchors.fill: parent
        color: learnOverlay.scrimColor
        opacity: 0.92
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.parent.width - 48, 560)
            height: cardContent.height + 48
            color: learnOverlay.cardColor
            border.width: 1
            border.color: learnOverlay.cardBorder
            radius: typeof themeStore !== "undefined" ? themeStore.radiusModal : 16

            Column {
                id: cardContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 18

                // Title
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hopOnLearnTitle : "Press your sequence"
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontHeading : 28
                    font.weight: Font.Bold
                    color: learnOverlay.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                // Live captured-tokens area.
                // Empty when nothing has been pressed yet — show a placeholder
                // em-dash so the user knows the area exists. Otherwise a Flow
                // wraps chips to multiple rows once the card width is full,
                // so a long sequence never overflows.
                Item {
                    id: chipRow
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    // Allow up to 3 rows of 36 px chips (108) + spacing.
                    height: Math.max(40, Math.min(120, chipFlow.implicitHeight))
                    clip: true

                    Text {
                        visible: learnOverlay.tokens.length === 0
                        anchors.centerIn: parent
                        text: "\u2014"
                        font.pixelSize: 24
                        color: learnOverlay.textSecondary
                    }

                    Flow {
                        id: chipFlow
                        visible: learnOverlay.tokens.length > 0
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Repeater {
                            model: learnOverlay.tokens
                            delegate: Rectangle {
                                width: chipText.implicitWidth + 14
                                height: 32
                                radius: 16
                                color: learnOverlay.chipColor
                                border.color: learnOverlay.accentColor
                                border.width: 1

                                // Fade-in for the most recent chip
                                opacity: 0
                                Component.onCompleted: opacity = 1
                                Behavior on opacity { NumberAnimation { duration: 150 } }

                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: learnOverlay.labelFor(modelData)
                                    color: learnOverlay.textPrimary
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }

                // Countdown text — shows remaining seconds (rounded up).
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.ceil(learnOverlay.idleMs / 1000) + " s"
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontHero : 48
                    font.weight: Font.Bold
                    color: learnOverlay.accentColor
                }

                // Hint
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined" ? translations.hopOnLearnHint : "Saves 5 s after the last press."
                    font.pixelSize: typeof themeStore !== "undefined" ? themeStore.fontBody : 18
                    color: learnOverlay.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }
    }
}
