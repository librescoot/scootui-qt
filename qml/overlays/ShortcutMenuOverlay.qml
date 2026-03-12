import QtQuick
import QtQuick.Layouts

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: shortcutMenuStore.visible

    property bool isDark: themeStore.isDark

    // Material Icons codepoints
    readonly property string miDarkMode:    "\ue1b0"
    readonly property string miLightMode:   "\ue37a"
    readonly property string miContrast:    "\uf04d8"
    readonly property string miMap:         "\uf1ae"
    readonly property string miTimer:       "\ue662"
    readonly property string miWarning:     "\ue6cc"
    readonly property string miBugReport:   "\ue115"

    // Main bottom container
    Rectangle {
        id: containerBg
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        
        // Flutter uses left/right 40, here we center and use implicit width
        width: contentRow.width + 40
        height: 120
        radius: 20
        
        color: isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.9)
        border.width: 2
        border.color: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.3)

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 20 // Space between items

            Repeater {
                model: 4 // Themes, View, Hazards, Debug
                
                Rectangle {
                    id: menuItemRect
                    property bool isSelected: index === shortcutMenuStore.selectedIndex
                    property color itemColor: isSelected ? "#FF9800" : (isDark ? "#FFFFFF" : "#212121")

                    width: isSelected ? 80 : 60
                    height: isSelected ? 80 : 60
                    radius: 16
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
                            switch(index) {
                                case 0: // Theme
                                    if (themeStore.isAutoMode) return miDarkMode
                                    if (themeStore.isDark) return miLightMode
                                    return miContrast
                                case 1: // View
                                    return screenStore.currentScreen === 0 ? miMap : miTimer
                                case 2: // Hazards
                                    return miWarning
                                case 3: // Debug
                                    return miBugReport
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

            // Progress bar container
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 4
                radius: 2
                color: isDark ? "#3DFFFFFF" : "#1F000000"

                // Active progress bar (shrinks from 1.0 to 0.0 over 1s)
                Rectangle {
                    anchors.left: parent.left
                    height: parent.height
                    radius: 2
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
