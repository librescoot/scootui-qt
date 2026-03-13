import QtQuick
import QtQuick.Effects

Item {
    id: root

    property alias source: img.source
    property alias sourceSize: img.sourceSize
    property alias fillMode: img.fillMode
    property color tintColor: "#FFFFFF"
    property bool tintEnabled: true

    Image {
        id: img
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        visible: !root.tintEnabled
    }

    MultiEffect {
        source: img
        anchors.fill: parent
        visible: root.tintEnabled
        colorization: 1.0
        colorizationColor: root.tintColor
    }
}
