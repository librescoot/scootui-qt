import QtQuick

Item {
    id: toastOverlay
    anchors.fill: parent
    z: 900

    property var toastModel: typeof toastService !== "undefined" ? toastService.toasts : []

    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 48
        spacing: 6

        Repeater {
            model: toastOverlay.toastModel

            delegate: Rectangle {
                id: toastItem
                width: Math.min(toastText.implicitWidth + 32, toastOverlay.width - 40)
                height: toastText.implicitHeight + 16
                radius: themeStore.radiusCard
                opacity: 0

                color: {
                    switch (modelData.type) {
                        case "error": return "#D32F2F"
                        case "warning": return "#F57C00"
                        case "success": return "#2E7D32"
                        default: return "#1976D2"   // info
                    }
                }

                Text {
                    id: toastText
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, toastOverlay.width - 72)
                    text: modelData.message
                    color: modelData.type === "warning" ? "#000000" : "white"
                    font.pixelSize: themeStore.fontBody
                    font.weight: Font.Medium
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Component.onCompleted: {
                    opacity = 1
                }

                Behavior on opacity {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
