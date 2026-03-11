import QtQuick
import QtQuick.Layouts

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: shortcutMenuStore.visible

    property bool isDark: themeStore.isDark

    // Container positioned at bottom center
    Item {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        width: containerBg.width
        height: 120

        // Container background with border
        Rectangle {
            id: containerBg
            anchors.centerIn: parent
            width: containerRow.width + 40
            height: 120
            radius: 20
            color: isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
            border.width: 2
            border.color: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.3)
        }

        Row {
            id: containerRow
            anchors.centerIn: parent
            spacing: 16

            Repeater {
                model: [
                    { icon: "\u263C", label: "Theme" },
                    { icon: "\u25A3", label: "View" },
                    { icon: "\u26A0", label: "Hazards" }
                ]

                Rectangle {
                    id: menuItemRect
                    property bool isSelected: index === shortcutMenuStore.selectedIndex

                    width: isSelected ? 80 : 60
                    height: isSelected ? 80 : 60
                    radius: 16
                    color: isSelected ? Qt.rgba(1, 0.6, 0, 0.15) : "transparent"
                    border.width: isSelected ? 4 : 2
                    border.color: isSelected ? "#FF9800"
                                  : (isDark ? "#FFFFFF" : "#000000")

                    Behavior on width {
                        NumberAnimation { duration: 200 }
                    }
                    Behavior on height {
                        NumberAnimation { duration: 200 }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: modelData.icon
                        font.pixelSize: menuItemRect.isSelected ? 36 : 28
                        color: menuItemRect.isSelected ? "#FF9800"
                               : (isDark ? "#FFFFFF" : "#000000")

                        Behavior on font.pixelSize {
                            NumberAnimation { duration: 200 }
                        }
                    }
                }
            }
        }
    }

    // Confirmation UI
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        width: parent.width - 120
        height: confirmCol.height + 32
        radius: 12
        color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        border.width: 2
        border.color: "#FF9800"
        visible: shortcutMenuStore.confirming

        Column {
            id: confirmCol
            anchors.centerIn: parent
            spacing: 12

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: typeof translations !== "undefined" ? translations.shortcutPressToConfirm : "Press to confirm"
                font.pixelSize: 16
                font.bold: true
                color: isDark ? "#FFFFFF" : "#000000"
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 4
                radius: 2
                color: isDark ? "#3DFFFFFF" : "#1F000000"

                Rectangle {
                    anchors.left: parent.left
                    height: parent.height
                    radius: 2
                    color: "#FF9800"
                    width: parent.width * (1.0 - confirmTimer.progress)
                }
            }
        }

        // Confirmation timer animation
        Timer {
            id: confirmTimer
            property real progress: 0
            interval: 16
            running: shortcutMenuStore.confirming
            repeat: true
            onTriggered: {
                progress = Math.min(progress + 0.016, 1.0)
            }
        }

        onVisibleChanged: {
            if (visible) confirmTimer.progress = 0
        }
    }
}
