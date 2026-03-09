import QtQuick

Item {
    id: svgIcon

    property alias source: image.source
    property color color: "#FFFFFF"
    property alias fillMode: image.fillMode

    width: 24
    height: 24

    Image {
        id: image
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
    }
}
