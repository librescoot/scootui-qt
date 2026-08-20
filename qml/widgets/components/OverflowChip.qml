import QtQuick
import ScootUI 1.0

// "…" chip with a small "+N" count: stands in for lower-priority status
// glyphs the top bar hid to avoid running into the clock. Display-only;
// full state is available in the parked detail view.
Item {
    id: chip

    property int count: 0
    property color iconColor: "#FFFFFF"

    visible: count > 0
    width: 24
    height: 24

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        text: "…"
        font.pixelSize: 18
        font.weight: Font.Bold
        color: chip.iconColor
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        text: "+" + chip.count
        font.pixelSize: ThemeStore.fontMicro
        font.weight: Font.Bold
        color: chip.iconColor
    }
}
