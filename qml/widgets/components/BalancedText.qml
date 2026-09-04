import QtQuick

// Word-wrapped Text that balances its lines instead of greedily filling each
// one before wrapping - so two roughly-even lines rather than one long line
// and a lone short word left dangling on the last one. Set `maxWidth`
// instead of `width`; the balanced width is derived from it.
//
// Works by binary-searching for the narrowest width that still produces the
// same line count as wrapping at maxWidth (the standard text-wrap: balance
// approach), using a hidden Text to probe candidate widths.
Text {
    id: root
    wrapMode: Text.WordWrap

    property real maxWidth: implicitWidth
    width: Math.min(implicitWidth, maxWidth)

    onImplicitWidthChanged: rebalance()
    onMaxWidthChanged: rebalance()
    onTextChanged: rebalance()
    onFontChanged: rebalance()
    Component.onCompleted: rebalance()

    function rebalance() {
        if (implicitWidth <= maxWidth) {
            width = implicitWidth
            return
        }

        probe.font = font
        probe.text = text
        probe.width = maxWidth
        var targetLines = probe.lineCount
        if (targetLines <= 1) {
            width = maxWidth
            return
        }

        var lo = 0
        var hi = maxWidth
        for (var i = 0; i < 14 && (hi - lo) > 1; i++) {
            var mid = Math.round((lo + hi) / 2)
            probe.width = mid
            if (probe.lineCount <= targetLines)
                hi = mid
            else
                lo = mid
        }
        width = hi
    }

    Text {
        id: probe
        visible: false
        wrapMode: Text.WordWrap
    }
}
