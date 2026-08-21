import QtQuick
import QtQuick.Layouts
import ScootUI 1.0

Item {
    id: shortcutOverlay
    anchors.fill: parent
    visible: ShortcutMenuStore.visible

    property Item blurSource
    property bool isDark: ThemeStore.isDark

    // Caption for a slot. The theme and view icons show the state they switch
    // to rather than the current one, so these name the target as well.
    function itemLabel(idx) {
        if (typeof Translations === "undefined")
            return ""
        switch (idx) {
        case 0:
            if (ThemeStore.isAutoMode) return Translations.shortcutThemeDark
            if (ThemeStore.isDark) return Translations.shortcutThemeLight
            return Translations.shortcutThemeAuto
        case 1:
            return ScreenStore.currentScreen === 0 ? Translations.shortcutViewMap
                                                   : Translations.shortcutViewCluster
        case 2: return Translations.shortcutToggleHazards
        case 3: return Translations.shortcutDebugOverlay
        default: return ""
        }
    }

    readonly property string selectedLabel: itemLabel(ShortcutMenuStore.selectedIndex)

    // Main bottom container. Fixed width, inset 40 on each side: sizing it to
    // the content instead meant the bar grew and shrank by 20 px as the
    // selection moved, and being centre-anchored it re-centred too, so every
    // icon slid sideways on each step.
    Item {
        id: containerWrapper
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 40
        anchors.rightMargin: 40
        anchors.bottom: parent.bottom
        // Sit clear of the confirmation bar rather than at a fixed offset:
        // hardcoding this let the confirm bar ride up over the caption line.
        // Derived, so the two never overlap and the icon bar does not jump
        // when confirmation starts.
        anchors.bottomMargin: confirmBar.anchors.bottomMargin + confirmBar.height + 6

        // 120 plus a caption line for the highlighted slot.
        height: 150
        clip: true

        FrostedGlass {
            anchors.fill: parent
            sourceItem: shortcutOverlay.blurSource
            sourceOffset: Qt.point(containerWrapper.x, containerWrapper.y)
            blurAmount: 0.5
            tintColor: isDark
                ? Qt.rgba(0, 0, 0, 0.5)
                : Qt.rgba(1, 1, 1, 0.55)
        }

        Rectangle {
            id: containerBg
            anchors.fill: parent
            radius: ThemeStore.radiusModal
            color: "transparent"
            border.width: 2
            border.color: isDark ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.3)
        }

        Row {
            id: contentRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            height: 80

            Repeater {
                model: 4 // Themes, View, Hazards, Debug

                // Equal-width cell per item, so the box can still grow on
                // selection without displacing its neighbours.
                Item {
                    width: contentRow.width / 4
                    height: contentRow.height

                    Rectangle {
                        id: menuItemRect
                        anchors.centerIn: parent
                        property bool isSelected: index === ShortcutMenuStore.selectedIndex
                        property color itemColor: isSelected ? "#FF9800" : (isDark ? "#FFFFFF" : "#212121")

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
                                switch(index) {
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

        // Caption for the highlighted slot, on its own line under the icons.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: contentRow.bottom
            anchors.topMargin: 6
            width: parent.width - 24
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: shortcutOverlay.selectedLabel
            font.pixelSize: ThemeStore.fontBody
            font.weight: Font.Medium
            color: isDark ? "#FFFFFF" : "#212121"
        }
    }

    // Confirmation prompt. Aligned to the icon bar above it rather than its own
    // width, and the countdown runs flush along the bottom edge instead of
    // floating as a short centred stub.
    Rectangle {
        id: confirmBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: containerWrapper.width
        height: confirmContent.height + 22
        radius: ThemeStore.radiusModal
        color: isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.95)
        border.width: 2
        border.color: "#FF9800"
        visible: ShortcutMenuStore.confirming

        Column {
            id: confirmContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                TintedImage {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    source: "qrc:/ScootUI/assets/icons/librescoot-seatbox-button.svg"
                    tintColor: "#FF9800"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: typeof Translations !== "undefined"
                          ? Translations.shortcutToConfirm : "to confirm"
                    font.pixelSize: ThemeStore.fontBody
                    font.weight: Font.Bold
                    color: isDark ? "#FFFFFF" : "#000000"
                }
            }

            // Countdown. Inset with the rest of the content rather than run
            // flush along the bottom edge, where it was easy to miss.
            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: isDark ? "#3DFFFFFF" : "#1F000000"

                // Drains left to right over the confirm timeout
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * (1.0 - confirmProgress)
                    radius: 3
                    color: "#FF9800"

                    // No initialiser: an initial binding here would compete
                    // with the NumberAnimation value source below. real
                    // starts at 0.
                    property real confirmProgress

                    NumberAnimation on confirmProgress {
                        id: confirmAnim
                        from: 0
                        to: 1
                        duration: ShortcutMenuStore.confirmTimeoutMs
                    }
                }
            }
        }

        onVisibleChanged: {
            if (visible) {
                confirmAnim.stop()
                confirmAnim.start()
            }
        }
    }
}
