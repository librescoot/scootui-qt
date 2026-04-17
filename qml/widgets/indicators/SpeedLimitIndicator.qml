import QtQuick
import ScootUI 1.0

Item {
    id: root
    property real iconSize: 35
    property string speedLimit: SpeedLimitStore.speedLimit

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
