import QtQuick

// Brake-lever hints for the bottom bar.
//
// Each hint names its gesture in a capsule in front of the action: TAP, HOLD,
// or HOLD 3S. Which lever a hint belongs to is carried by the edge it sits
// against, as it always was, so the "Left Brake" caption every screen used to
// print is gone.
//
// The three durations are row slots shared by both sides, so taps line up
// across the bar and holds line up under them. A slot neither side has bound
// collapses, and the bar sizes to what is left: a screen never shows a row for
// a gesture that does nothing.
Item {
    id: controlHints

    property string leftTap: ""
    property string leftHold: ""
    property string rightTap: ""
    property string rightHold: ""

    // The 3 s hold. Reserved for close and cancel, so the long hold means the
    // same thing on every screen that binds it.
    property string leftHoldLong: ""
    property string rightHoldLong: ""

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color labelColor: isDark ? "#FFFFFF" : "#000000"
    readonly property color wordColor: isDark ? "#8AFFFFFF" : "#8A000000"
    readonly property color wordInkColor: isDark ? "#000000" : "#FFFFFF"
    readonly property real capsulePad: 5
    // Qt counts one letterSpacing after the last character too, so a Text's
    // implicitWidth is a pixel wider than the ink. Centring the box would sit
    // the word that pixel left of centre in the capsule; the width is taken
    // off the box so the ink is what gets centred.
    readonly property real wordSpacing: 1
    // The ink box alone still leaves the capsule looking right-heavy on the
    // panel, so the right edge comes in by this much. Trimmed off the width
    // only: the word is placed from the left, so the left padding is untouched.
    readonly property real capsuleTrim: 1
    readonly property real wordSize: typeof themeStore !== "undefined" ? themeStore.fontMicro : 10

    // The duration leads the verb in German and trails it in English, so these
    // are three separate strings rather than one format with a number in it.
    readonly property string wordTap: typeof translations !== "undefined"
                                      ? translations.gestureTap : "TAP"
    readonly property string wordHold: typeof translations !== "undefined"
                                       ? translations.gestureHold : "HOLD"
    readonly property string wordHoldLong: typeof translations !== "undefined"
                                           ? translations.gestureHoldLong : "HOLD 3S"

    readonly property bool slotTap: leftTap !== "" || rightTap !== ""
    readonly property bool slotHold: leftHold !== "" || rightHold !== ""
    readonly property bool slotHoldLong: leftHoldLong !== "" || rightHoldLong !== ""
    readonly property int rowCount: (slotTap ? 1 : 0)
                                  + (slotHold ? 1 : 0)
                                  + (slotHoldLong ? 1 : 0)

    // Screens whose labels come and go with the scroll position pin the row
    // count here. Two reasons: a bar that changes height mid-scroll shifts the
    // content above it, and where the flickable is sized as
    // "parent.height - bar.height" a content-derived height is a binding loop.
    property int reservedRows: 0

    // One row keeps the 40 px the bar has always been; each further row costs
    // 19. Screens size their content off this, so it has to stay a binding.
    implicitHeight: Math.max(40, 15 + Math.max(rowCount, reservedRows) * 19)
    height: implicitHeight

    // The capsule runs straight into its verb rather than sitting in a column
    // sized to the widest word. Holding the verbs on one margin would leave
    // TAP 20 px from the word it belongs to in German, where HALTEN sits right
    // against its own; the capsule already reads as one object, so it does not
    // need a column to keep it from floating. The capsules all start at the
    // screen margin, which is the alignment that matters on a bar anchored to
    // the edge.
    component HintRow: Row {
        id: hintRow

        property string word: ""
        property string label: ""
        property bool mirrored: false

        height: 18
        spacing: 7
        layoutDirection: mirrored ? Qt.RightToLeft : Qt.LeftToRight

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: hintRow.label !== ""
            width: capsuleInk.tightBoundingRect.width + controlHints.capsulePad * 2
                   - controlHints.capsuleTrim
            height: 13
            radius: 6.5
            color: controlHints.wordColor

            // The advance box is not the ink. Qt counts a trailing
            // letterSpacing into implicitWidth, and a glyph carries its own
            // side bearings on top of that: P leaves more air to its right
            // than D does, so centring the box sat TAP visibly left of centre
            // while HOLD looked fine. tightBoundingRect is the ink itself, so
            // the capsule is sized and the word placed by that instead.
            TextMetrics {
                id: capsuleInk
                font: capsuleText.font
                text: hintRow.word
            }

            Text {
                id: capsuleText
                // Horizontal only. tightBoundingRect is baseline-relative, so
                // its y is no use for placing the item; the vertical centre of
                // an all-caps word is close enough on a 13 px capsule anyway.
                x: controlHints.capsulePad - capsuleInk.tightBoundingRect.x
                anchors.verticalCenter: parent.verticalCenter
                text: hintRow.word
                color: controlHints.wordInkColor
                font.pixelSize: controlHints.wordSize
                font.weight: Font.Bold
                font.letterSpacing: controlHints.wordSpacing
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: hintRow.label
            color: controlHints.labelColor
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        HintRow {
            visible: controlHints.slotTap
            word: controlHints.wordTap
            label: controlHints.leftTap
        }
        HintRow {
            visible: controlHints.slotHold
            word: controlHints.wordHold
            label: controlHints.leftHold
        }
        HintRow {
            visible: controlHints.slotHoldLong
            word: controlHints.wordHoldLong
            label: controlHints.leftHoldLong
        }
    }

    Column {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        HintRow {
            anchors.right: parent.right
            visible: controlHints.slotTap
            mirrored: true
            word: controlHints.wordTap
            label: controlHints.rightTap
        }
        HintRow {
            anchors.right: parent.right
            visible: controlHints.slotHold
            mirrored: true
            word: controlHints.wordHold
            label: controlHints.rightHold
        }
        HintRow {
            anchors.right: parent.right
            visible: controlHints.slotHoldLong
            mirrored: true
            word: controlHints.wordHoldLong
            label: controlHints.rightHoldLong
        }
    }
}
