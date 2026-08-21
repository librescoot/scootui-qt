import QtQuick
import ScootUI 1.0

// A calendar glyph carrying the day of the month on its page.
//
// The artwork is drawn in a 48-unit box: body 4..44 wide and 7..44 tall with a
// 3.5-unit stroke, a header band down to y=14, so the page the numeral sits on
// runs 7.5..40.5 across and 14..40.5 down. Every dimension below is that box
// times one scale factor, so the numeral cannot drift out of step with the
// artwork the way a separately tuned font size would.
Item {
    id: root

    property int day: 1
    property real iconSize: 26
    property color color: "#FFFFFF"

    readonly property real u: iconSize / 48
    readonly property real pageWidth: 33 * u
    readonly property real pageHeight: 26.5 * u
    // The page sits below the header band, so its centre is 3.25 units below
    // the centre of the box.
    readonly property real pageOffsetY: 3.25 * u
    readonly property real numeralSize: 22 * u

    width: iconSize
    height: iconSize

    SvgIcon {
        anchors.fill: parent
        source: "qrc:/ScootUI/assets/icons/calendar_blank.svg"
        color: root.color
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.pageOffsetY
        // 0.95 keeps a two-digit day off the stroke on either side.
        width: root.pageWidth * 0.95
        height: root.pageHeight
        fontSizeMode: Text.Fit
        minimumPixelSize: 6
        text: root.day
        font.family: "Roboto Condensed"
        font.pixelSize: root.numeralSize
        font.weight: Font.Bold
        font.features: {"tnum": 1}
        color: root.color
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
