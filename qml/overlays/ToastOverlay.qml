import QtQuick
import QtQuick.Effects
import ScootUI 1.0

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
                width: Math.min(contentRow.implicitWidth + 32, toastOverlay.width - 40)
                height: contentRow.implicitHeight + 16
                radius: ThemeStore.radiusCard
                opacity: 0

                readonly property bool hasIcon: modelData.icon !== undefined && modelData.icon !== ""
                readonly property color contentColor: modelData.type === "warning" ? "#000000" : "white"

                color: {
                    switch (modelData.type) {
                        case "error": return "#D32F2F"
                        case "warning": return "#F57C00"
                        case "success": return "#2E7D32"
                        default: return "#1976D2"   // info
                    }
                }

                Row {
                    id: contentRow
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        id: toastIcon
                        visible: toastItem.hasIcon
                        width: visible ? 22 : 0
                        height: 22
                        anchors.verticalCenter: parent.verticalCenter
                        source: toastItem.hasIcon ? modelData.icon : ""
                        sourceSize: Qt.size(22, 22)
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: visible
                        layer.effect: MultiEffect {
                            colorization: 1.0
                            colorizationColor: toastItem.contentColor
                        }
                    }

                    Text {
                        id: toastText
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, toastOverlay.width - 72 - (toastIcon.visible ? 30 : 0))
                        text: modelData.message
                        color: toastItem.contentColor
                        font.pixelSize: ThemeStore.fontBody
                        font.weight: Font.Medium
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
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
