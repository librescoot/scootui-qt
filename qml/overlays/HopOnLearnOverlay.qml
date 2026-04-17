import QtQuick
import ScootUI 1.0

// Full-screen overlay for the hop-on combo learning flow.
// Shows a live row of chips, one per captured token, growing left-to-right
// as the user presses brakes / horn / blinker switch / seatbox button.
// A countdown ticks from 5000 ms down; the buffer is committed when it
// reaches 0 (provided the user pressed at least 2 inputs).
Item {
    id: learnOverlay
    anchors.fill: parent

    // HopOnStore.Mode.Learning == 1
    readonly property int modeLearning: 1

    property int mode: HopOnStore.mode
    property var tokens: true ? HopOnStore.capturedTokens : []
    property int idleMs: HopOnStore.idleMillisRemaining

    visible: mode === modeLearning

    readonly property bool isDark: ThemeStore.isDark
    readonly property color scrimColor:    isDark ? "#000000" : "#FFFFFF"
    readonly property color cardColor:     isDark ? "#CC000000" : "#CCFFFFFF"
    readonly property color cardBorder:    isDark ? "#4DFFFFFF" : "#4D000000"
    readonly property color textPrimary:   isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#B3FFFFFF" : "#B3000000"
    readonly property color chipColor:     isDark ? "#1FFFFFFF" : "#1F000000"
    readonly property color accentColor:   "#FF9800"

    // Map a token id to its icon. Reuses existing librescoot-* SVGs
    // (turn-left/right) and the four new ones contributed for hop-on
    // (brake-left/right, horn, seatbox-button). Tinted via TintedImage
    // so the icon adapts to the active theme.
    function iconFor(token) {
        if (token === "LB")   return "qrc:/ScootUI/assets/icons/librescoot-brake-left.svg"
        if (token === "RB")   return "qrc:/ScootUI/assets/icons/librescoot-brake-right.svg"
        if (token === "HORN") return "qrc:/ScootUI/assets/icons/librescoot-horn.svg"
        if (token === "BL")   return "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
        if (token === "BR")   return "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
        if (token === "SBOX") return "qrc:/ScootUI/assets/icons/librescoot-seatbox-button.svg"
        return ""
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
            radius: ThemeStore.radiusModal

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
                    text: Translations.hopOnLearnTitle
                    font.pixelSize: ThemeStore.fontHeading
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
                    // Allow up to 3 rows of 40 px chips (120) + spacing.
                    height: Math.max(48, Math.min(140, chipFlow.implicitHeight))
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
                        spacing: 8

                        Repeater {
                            model: learnOverlay.tokens
                            delegate: Rectangle {
                                id: tokenChip
                                required property var modelData
                                width: 40
                                height: 40
                                radius: 20
                                color: learnOverlay.chipColor
                                border.color: learnOverlay.accentColor
                                border.width: 1

                                opacity: 0
                                Component.onCompleted: opacity = 1
                                Behavior on opacity { NumberAnimation { duration: 150 } }

                                TintedImage {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    source: learnOverlay.iconFor(tokenChip.modelData)
                                    tintColor: learnOverlay.textPrimary
                                }
                            }
                        }
                    }
                }

                // Countdown text — shows remaining seconds (rounded up).
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Math.ceil(learnOverlay.idleMs / 1000) + " s"
                    font.pixelSize: ThemeStore.fontHero
                    font.weight: Font.Bold
                    color: learnOverlay.accentColor
                }

                // Hint
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Translations.hopOnLearnHint
                    font.pixelSize: ThemeStore.fontBody
                    color: learnOverlay.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }
    }
}
