import QtQuick
import QtQuick.Effects

Item {
    id: svgIcon

    property alias source: image.source
    property color color: "#FFFFFF"
    property alias fillMode: image.fillMode
    property bool tintEnabled: true

    width: 24
    height: 24

    // Tint via a standalone MultiEffect (not layer.effect): its colorizationColor
    // is a live binding that re-fires on theme change. layer.effect one-shots the
    // color at build time on Qt 6.7, leaving icons stuck at the boot-time tint.
    // The source stays a hidden texture provider so the effect refreshes on load.
    Image {
        id: image
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        visible: !svgIcon.tintEnabled
        layer.enabled: svgIcon.tintEnabled
    }
    MultiEffect {
        anchors.fill: parent
        source: image
        visible: svgIcon.tintEnabled
        colorization: 1.0
        colorizationColor: svgIcon.color
    }
}
