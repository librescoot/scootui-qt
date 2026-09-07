import QtQuick
import QtQuick.Effects
import "../widgets/components"

Item {
    id: toastOverlay
    anchors.fill: parent
    z: 900

    property var toastModel: typeof toastService !== "undefined" ? toastService.toasts : []

    Column {
        anchors.top: parent.top
        anchors.topMargin: 48
        width: toastOverlay.width
        spacing: 6

        Repeater {
            model: toastOverlay.toastModel

            // Column positions each direct child itself, which silently
            // overrides any anchor a child sets on the axis it manages - a
            // known QtQuick positioner/anchors conflict. Rather than fight
            // it, this row spans the full width (so Column's left-alignment
            // is a no-op) and centers the actual pill one level down, where
            // Column no longer has a say. That keeps every toast centered on
            // its own instead of sharing a left edge with its neighbors.
            delegate: Item {
                id: toastDelegate
                width: parent.width
                height: toastItem.height

                Rectangle {
                    id: toastItem
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(contentRow.implicitWidth + 32, toastDelegate.width - 40)
                    height: contentRow.implicitHeight + 16
                    radius: themeStore.radiusCard
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

                        BalancedText {
                            id: toastText
                            anchors.verticalCenter: parent.verticalCenter
                            maxWidth: toastDelegate.width - 72 - (toastIcon.visible ? 30 : 0)
                            text: modelData.message
                            color: toastItem.contentColor
                            font.pixelSize: themeStore.fontBody
                            font.weight: Font.Medium
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
}
