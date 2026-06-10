import QtQuick

Item {
    id: root
    property real iconSize: 35
    property string speedLimit: typeof speedLimitStore !== "undefined"
                                ? speedLimitStore.speedLimit : ""

    // Only numeric values (e.g. "30", "50") are displayable. OSM maxspeed may
    // contain non-numeric tokens like "signals", "variable", "walk" — filter
    // those out and show nothing rather than overflowing text.
    readonly property bool isNumeric: /^\d+$/.test(speedLimit)

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
        // Bound to the inner white circle of the sign SVG so 3-digit limits
        // shrink to fit instead of overflowing the red ring.
        width: root.iconSize * 0.7
        height: root.iconSize * 0.7
        fontSizeMode: Text.Fit
        minimumPixelSize: 8
        visible: root.isNumeric
        text: root.speedLimit
        font.family: "Roboto Condensed"
        font.pixelSize: root.iconSize * (72 / 144)
        font.weight: Font.Bold
        color: "black"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
