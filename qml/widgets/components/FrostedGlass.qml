import QtQuick
import QtQuick.Effects

Item {
    id: root

    property Item sourceItem
    property real blurAmount: 0.6
    property color tintColor: Qt.rgba(0, 0, 0, 0.65)
    property real radius: 0
    // Explicit offset into the sourceItem's coordinate space.
    // For full-screen overlays this defaults to (0,0).
    // For positioned containers, set to the container's position
    // relative to the sourceItem (e.g. Qt.point(container.x, container.y)).
    property point sourceOffset: Qt.point(0, 0)

    // How often the backdrop is resampled while this is on screen.
    //
    // A live ShaderEffectSource re-renders its whole source subtree every
    // frame. The source here is the screen loader, so with a menu open over
    // the map that draws the entire map a second time per frame, on top of
    // the ~58 ms the map already costs the GC880 to draw once. What that buys
    // is a backdrop that is blurred to mush and then covered by a 0.65 tint.
    // Resampling a few times a second instead is not something you can see,
    // and it takes the second map render off the per-frame path.
    property int refreshIntervalMs: 200

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
        live: false
    }

    // triggeredOnStart so the backdrop is current the moment the overlay
    // appears, rather than showing whatever was last sampled.
    Timer {
        running: root.visible && root.sourceItem !== null
        interval: root.refreshIntervalMs
        repeat: true
        triggeredOnStart: true
        onTriggered: effectSource.scheduleUpdate()
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: root.radius > 0
    }

    MultiEffect {
        anchors.fill: parent
        source: effectSource
        blurEnabled: true
        blur: root.blurAmount
        maskEnabled: root.radius > 0
        maskSource: mask
        // Each step of blurMax is another downscale/upscale pass. 64 is a
        // radius of ~38 px at the default blurAmount, which is 8% of a 480 px
        // panel: far more spread than the effect reads as, for twice the
        // passes.
        blurMax: 32
        visible: root.sourceItem !== null
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.tintColor
    }
}
