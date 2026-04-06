import QtQuick
import QtQuick.Effects
import ScootUI

Item {
    id: svgIcon

    property alias source: image.source
    property color color: "#FFFFFF"
    property alias fillMode: image.fillMode
    property bool tintEnabled: true

    width: 24
    height: 24

    Image {
        id: image
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        visible: !svgIcon.tintEnabled
    }

    MultiEffect {
        source: image
        anchors.fill: parent
        visible: svgIcon.tintEnabled
        colorization: 1.0
        colorizationColor: svgIcon.color
    }
}
