import QtQuick
import QtQuick.Effects
import ScootUI

Item {
    id: root

    property Item sourceItem
    property real blurAmount: 0.6
    property color tintColor: Qt.rgba(0, 0, 0, 0.65)
    // Explicit offset into the sourceItem's coordinate space.
    // For full-screen overlays this defaults to (0,0).
    // For positioned containers, set to the container's position
    // relative to the sourceItem (e.g. Qt.point(container.x, container.y)).
    property point sourceOffset: Qt.point(0, 0)

    ShaderEffectSource {
        id: effectSource
        anchors.fill: parent
        sourceItem: root.sourceItem
        sourceRect: {
            if (!root.sourceItem || root.width <= 0 || root.height <= 0)
                return Qt.rect(0, 0, 0, 0)
            return Qt.rect(root.sourceOffset.x, root.sourceOffset.y,
                           root.width, root.height)
        }
        visible: false
        live: true
    }

    MultiEffect {
        anchors.fill: parent
        source: effectSource
        blurEnabled: true
        blur: root.blurAmount
        blurMax: 64
        visible: root.sourceItem !== null
    }

    Rectangle {
        anchors.fill: parent
        color: root.tintColor
    }
}
