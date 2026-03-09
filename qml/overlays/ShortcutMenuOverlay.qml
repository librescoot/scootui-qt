import QtQuick
import QtQuick.Layouts

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: shortcutMenuStore.visible

    // Positioned at bottom center
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        spacing: 12

        Repeater {
            model: [
                { icon: "\u263C", label: "Theme" },
                { icon: "\u25A3", label: "View" },
                { icon: "\u26A0", label: "Hazards" }
            ]

            Rectangle {
                width: 48
                height: 48
                radius: 24
                color: themeStore.isDark ? "#33FFFFFF" : "#33000000"
                border.width: index === shortcutMenuStore.selectedIndex ? 3 : 0
                border.color: "#FF9800"

                scale: index === shortcutMenuStore.selectedIndex && shortcutMenuStore.confirming ? 1.2 : 1.0
                Behavior on scale {
                    NumberAnimation { duration: 150 }
                }

                Text {
                    anchors.centerIn: parent
                    text: modelData.icon
                    font.pixelSize: 20
                    color: themeStore.isDark ? "#FFFFFF" : "#000000"
                }
            }
        }
    }
}
