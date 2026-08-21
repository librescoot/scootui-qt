import QtQuick
import ScootUI 1.0

Item {
    id: root
    property real iconSize: 35
    property string speedLimit: typeof SpeedLimitStore !== "undefined"
                                ? SpeedLimitStore.speedLimit : ""

    // Only numeric values (e.g. "30", "50") are displayable. OSM maxspeed may
    // contain non-numeric tokens like "signals", "variable", "walk" — filter
    // those out and show nothing rather than overflowing text.
    readonly property bool isNumeric: /^\d+$/.test(speedLimit)

    // The sign artwork is drawn in a 144-unit box, concentric on its centre:
    // outer disc 130, white field 102, so the red ring is 14 wide, 1/9.3 of the
    // outer diameter as in StVO 274 / Vienna C14. Numeral cap height is 72.
    // Everything below is that box times one scale factor, so no dimension can
    // drift out of step with the artwork.
    readonly property real u: iconSize / 144
    readonly property real fieldSize: 102 * u
    readonly property real numeralSize: 72 * u

    visible: isNumeric || speedLimit === "none"
    width: iconSize
    height: iconSize

    Image {
        anchors.fill: parent
        source: {
            if (root.speedLimit === "none")
                return "qrc:/ScootUI/assets/icons/speedlimit_none.svg"
            return "qrc:/ScootUI/assets/icons/speedlimit_blank.svg"
        }
        sourceSize: Qt.size(root.iconSize, root.iconSize)
        fillMode: Image.PreserveAspectFit
    }

    Text {
        anchors.centerIn: parent
        // Fit to the white field, not to the icon box. The corners of the text
        // box fall outside the circle, so a box as wide as the field lets a
        // three-digit limit crowd the ring; 0.9 keeps the widest case clear.
        width: root.fieldSize * 0.9
        height: root.fieldSize
        fontSizeMode: Text.Fit
        minimumPixelSize: 8
        visible: root.isNumeric
        text: root.speedLimit
        font.family: "Roboto Condensed"
        font.pixelSize: root.numeralSize
        font.weight: Font.Bold
        color: "black"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
