import QtQuick

Item {
    id: blinkerOverlay
    anchors.fill: parent

    readonly property int blinkerState: typeof vehicleStore !== "undefined" ? vehicleStore.blinkerState : 0
    readonly property bool overlayEnabled: typeof settingsStore !== "undefined" ? settingsStore.blinkerStyle === "overlay" : false
    readonly property bool showLeft: overlayEnabled && (blinkerState === 1 || blinkerState === 3)
    readonly property bool showRight: overlayEnabled && (blinkerState === 2 || blinkerState === 3)

    visible: showLeft || showRight

    // Left arrow
    Image {
        visible: showLeft
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 80
        height: 120
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-left.svg"
        sourceSize: Qt.size(80, 120)
        fillMode: Image.PreserveAspectFit

        SequentialAnimation on opacity {
            running: showLeft
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 1; duration: 250; easing.type: Easing.InOutExpo }
            NumberAnimation { from: 1; to: 0; duration: 250; easing.type: Easing.InOutExpo }
            PauseAnimation { duration: 228 }
        }
    }

    // Right arrow
    Image {
        visible: showRight
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 80
        height: 120
        source: "qrc:/ScootUI/assets/icons/librescoot-turn-right.svg"
        sourceSize: Qt.size(80, 120)
        fillMode: Image.PreserveAspectFit

        SequentialAnimation on opacity {
            running: showRight
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 1; duration: 250; easing.type: Easing.InOutExpo }
            NumberAnimation { from: 1; to: 0; duration: 250; easing.type: Easing.InOutExpo }
            PauseAnimation { duration: 228 }
        }
    }
}
