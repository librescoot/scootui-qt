import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: ShortcutMenuStore.visible

    property Item blurSource

    // Main bottom container
    Item {
        id: containerWrapper
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60

        width: contentRow.width + 40
        height: 120
        clip: true

        FrostedGlass {
            anchors.fill: parent
            sourceItem: shortcutOverlay.blurSource
            sourceOffset: Qt.point(containerWrapper.x, containerWrapper.y)
            blurAmount: 0.5
            tintColor: ThemeStore.isDark
                ? Qt.rgba(0, 0, 0, 0.5)
                : Qt.rgba(1, 1, 1, 0.55)
        }

        Rectangle {
            id: containerBg
            anchors.fill: parent
            radius: ThemeStore.radiusModal
            color: "transparent"
            border.width: 2
            border.color: ThemeStore.isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.3)
        }

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 20 // Space between items

            Repeater {
                model: 4 // Themes, View, Hazards, Debug
                
                Rectangle {
                    id: menuItemRect
                    required property int index
                    property bool isSelected: menuItemRect.index === ShortcutMenuStore.selectedIndex
                    property color itemColor: isSelected ? "#FF9800" : (ThemeStore.isDark ? "#FFFFFF" : "#212121")

                    width: isSelected ? 80 : 60
                    height: isSelected ? 80 : 60
                    radius: ThemeStore.radiusModal
                    color: isSelected ? Qt.rgba(1, 0.6, 0, 0.15) : "transparent"
                    border.width: isSelected ? 4 : 2
                    border.color: itemColor

                    Behavior on width { NumberAnimation { duration: 200 } }
                    Behavior on height { NumberAnimation { duration: 200 } }

                    Text {
                        anchors.centerIn: parent
                        font.family: "Material Icons"
                        font.pixelSize: menuItemRect.isSelected ? 36 : 28
                        color: menuItemRect.itemColor
                        text: {
                            switch(menuItemRect.index) {
                                case 0: // Theme
                                    if (ThemeStore.isAutoMode) return MaterialIcon.iconDarkMode
                                    if (ThemeStore.isDark) return MaterialIcon.iconLightMode
                                    return MaterialIcon.iconContrast
                                case 1: // View
                                    return ScreenStore.currentScreen === 0 ? MaterialIcon.iconMap : MaterialIcon.iconSpeed
                                case 2: // Hazards
                                    return MaterialIcon.iconWarningAmber
                                case 3: // Debug
                                    return MaterialIcon.iconBugReport
                                default: return ""
                            }
                        }

                        Behavior on font.pixelSize { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }
    }

    // Confirmation UI (Bottom-most bar)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        // Matching Flutter's left/right 60
        width: Math.min(parent.width - 120, 360)
        height: confirmCol.height + 32
        radius: ThemeStore.radiusModal
        color: ThemeStore.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        border.width: 2
        border.color: "#FF9800"
        visible: ShortcutMenuStore.confirming

        Column {
            id: confirmCol
            anchors.centerIn: parent
            spacing: 12

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Translations.shortcutPressToConfirm
                font.pixelSize: ThemeStore.fontBody
                font.weight: Font.Bold
                color: ThemeStore.isDark ? "#FFFFFF" : "#000000"
            }

            // Progress bar container
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 4
                radius: ThemeStore.radiusBar
                color: ThemeStore.isDark ? "#3DFFFFFF" : "#1F000000"

                // Active progress bar (shrinks from 1.0 to 0.0 over 1s)
                Rectangle {
                    anchors.left: parent.left
                    height: parent.height
                    radius: ThemeStore.radiusBar
                    color: "#FF9800"
                    width: parent.width * (1.0 - confirmTimer.progress)
                }
            }
        }

        // Confirmation timer (1s duration matching CONFIRM_TIMEOUT_MS)
        Timer {
            id: confirmTimer
            property real progress: 0
            interval: 16
            running: ShortcutMenuStore.confirming
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
