import QtQuick

Item {
    id: root
    property real iconSize: 35
    property string speedLimit: typeof speedLimitStore !== "undefined"
                                ? speedLimitStore.speedLimit : ""

    visible: speedLimit.length > 0 && speedLimit !== "unknown"
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
        visible: root.speedLimit !== "none" && root.speedLimit !== "unknown"
        text: root.speedLimit
        font.pixelSize: root.iconSize * (72 / 144)
        font.bold: true
        color: "black"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
