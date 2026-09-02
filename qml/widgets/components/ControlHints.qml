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

    // Shorter stand-in for leftHold, used only when the full label would run
    // into the right hint. Leave it empty to say there is no shorter form.
    property string leftHoldShort: ""

    // The 3 s hold. Reserved for close and cancel, so the long hold means the
    // same thing on every screen that binds it.
    property string leftHoldLong: ""
    property string rightHoldLong: ""

    // Follows the theme unless the screen pins it: a screen whose background
    // is black in both themes has to say so, or the light palette paints the
    // capsules and labels black on black.
    property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
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

    // One row per duration, shared by both levers. Left and right live in the
    // same row item rather than in two columns that happen to be centred the
    // same way: with separate columns nothing structurally ties a left hint to
    // its right counterpart, and the right side drifted to the middle of a
    // two-row bar. Here a hint cannot be on a different line from its partner.
    component SlotRow: Item {
        id: slotRow

        property string word: ""
        property string leftLabel: ""
        // Used in place of leftLabel when the pair would not fit.
        property string leftLabelShort: ""
        property string rightLabel: ""

        height: 18

        // Both sides anchor to opposite edges, so an over-long pair meets in
        // the middle rather than pushing each other out. German finds this
        // first: "Zurueck: Einstellungen" against "Jetzt auf Updates pruefen"
        // overruns a 480 px bar by a comfortable margin.
        //
        // Measured off the full labels rather than off the rendered groups,
        // which would make the width depend on the label that depends on the
        // width. Both sides carry the same word, so one capsule measurement
        // does for both.
        TextMetrics {
            id: rowWordInk
            font.pixelSize: controlHints.wordSize
            font.weight: Font.Bold
            font.letterSpacing: controlHints.wordSpacing
            text: slotRow.word
        }
        TextMetrics {
            id: leftInk
            font.pixelSize: 17
            font.weight: Font.DemiBold
            text: slotRow.leftLabel
        }
        TextMetrics {
            id: rightInk
            font.pixelSize: 17
            font.weight: Font.DemiBold
            text: slotRow.rightLabel
        }

        readonly property real capsuleWidth: rowWordInk.tightBoundingRect.width
                                             + controlHints.capsulePad * 2
                                             - controlHints.capsuleTrim
        // 7 is HintGroup's capsule-to-label spacing; 12 is the smallest gap
        // that still reads as two separate hints rather than one run of text.
        readonly property bool cramped: rightLabel !== "" && width > 0
            && capsuleWidth * 2 + 14 + leftInk.width + rightInk.width + 12 > width

        HintGroup {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            word: slotRow.word
            // The right hint names an action the rider may not know exists;
            // the left one restates the gesture they just used to get here.
            // So the left one gives way.
            label: (slotRow.cramped && slotRow.leftLabelShort !== "")
                   ? slotRow.leftLabelShort : slotRow.leftLabel
        }

        HintGroup {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            mirrored: true
            word: slotRow.word
            label: slotRow.rightLabel
        }
    }

    // The capsule runs straight into its verb rather than sitting in a column
    // sized to the widest word. Holding the verbs on one margin would leave
    // TAP 20 px from the word it belongs to in German, where HALTEN sits right
    // against its own; the capsule already reads as one object, so it does not
    // need a column to keep it from floating. The capsules all start at the
    // screen margin, which is the alignment that matters on a bar anchored to
    // the edge.
    component HintGroup: Row {
        id: hintRow

        property string word: ""
        property string label: ""
        property bool mirrored: false

        height: 18
        spacing: 7
        visible: label !== ""
        layoutDirection: mirrored ? Qt.RightToLeft : Qt.LeftToRight

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: capsuleInk.tightBoundingRect.width + controlHints.capsulePad * 2
                   - controlHints.capsuleTrim
            height: 13
            radius: 6.5
            color: controlHints.wordColor

            // The advance box is not the ink. Qt counts a trailing
            // letterSpacing into implicitWidth, and a glyph carries its own
            // side bearings on top of that: P leaves more air to its right
            // than D does. tightBoundingRect is the ink itself, so the capsule
            // is sized and the word placed by that instead.
            TextMetrics {
                id: capsuleInk
                font: capsuleText.font
                text: hintRow.word
            }

            Text {
                id: capsuleText
                // Horizontal only. tightBoundingRect is baseline-relative, so
                // its y is no use for placing the item; the vertical centre of
                // an all-caps word is close enough on a 13 px capsule.
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
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        SlotRow {
            width: parent.width
            visible: controlHints.slotTap
            word: controlHints.wordTap
            leftLabel: controlHints.leftTap
            rightLabel: controlHints.rightTap
        }
        SlotRow {
            width: parent.width
            visible: controlHints.slotHold
            word: controlHints.wordHold
            leftLabel: controlHints.leftHold
            leftLabelShort: controlHints.leftHoldShort
            rightLabel: controlHints.rightHold
        }
        SlotRow {
            width: parent.width
            visible: controlHints.slotHoldLong
            word: controlHints.wordHoldLong
            leftLabel: controlHints.leftHoldLong
            rightLabel: controlHints.rightHoldLong
        }
    }
}
