import QtQuick
import QtQuick.Effects

Item {
    id: root

    property alias source: img.source
    property alias sourceSize: img.sourceSize
    property alias fillMode: img.fillMode
    property color tintColor: "#FFFFFF"
    property bool tintEnabled: true

    // Standalone MultiEffect so colorizationColor stays a live binding (layer.effect
    // one-shots it on Qt 6.7). Source is a hidden texture provider so the effect
    // still refreshes when the SVG finishes loading.
    Image {
        id: img
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        visible: !root.tintEnabled
        layer.enabled: root.tintEnabled
    }
    MultiEffect {
        anchors.fill: parent
        source: img
        visible: root.tintEnabled
        colorization: 1.0
        colorizationColor: root.tintColor
    }
}
