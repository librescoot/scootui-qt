import QtQuick

Item {
    id: indicatorLight

    property alias source: icon.source
    property bool blinking: false
    property bool active: false
    property color tintColor: "#FFFFFF"

    width: 32
    height: 32
    visible: active

    Image {
        id: icon
        anchors.fill: parent
        sourceSize: Qt.size(parent.width, parent.height)
        fillMode: Image.PreserveAspectFit
        opacity: blinking ? blinkOpacity : 1.0

        property real blinkOpacity: 0

        SequentialAnimation on blinkOpacity {
            running: blinking && active
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 1; duration: 250; easing.type: Easing.InOutExpo }
            NumberAnimation { from: 1; to: 0; duration: 250; easing.type: Easing.InOutExpo }
            PauseAnimation { duration: 228 }
        }
    }
}
