import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Item {
    id: menuOverlay
    anchors.fill: parent
    visible: opacity > 0
    opacity: menuStore.isOpen ? 1.0 : 0.0

    property Item blurSource

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    FrostedGlass {
        anchors.fill: parent
        sourceItem: menuOverlay.blurSource
        blurAmount: 0.6
        tintColor: themeStore.isDark
            ? Qt.rgba(0, 0, 0, 0.65)
            : Qt.rgba(1, 1, 1, 0.65)
    }

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        enabled: menuStore.isOpen
        function onLeftTap()  { menuStore.navigateDown() }
        function onLeftHold() { menuStore.goBack()        }
        function onRightTap() { menuStore.selectItem()   }
        // Right long-tap runs the selected row's primary action without
        // entering it. Only ever bound to something cheap to undo: at 800 ms
        // a deliberate but slow select crosses the same threshold.
        function onRightHold() { menuStore.activatePrimary() }
        // The 3 s hold leaves the menu from any depth, deliberately without a
        // row in the bar: an escape from four levels down is worth having and
        // is not worth 19 px of every screen to advertise. It arrives after
        // the 800 ms long-tap has already popped one level, so from the root
        // the menu has closed before this can fire.
        function onLeftBrakeHold() { menuStore.close() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 24
        spacing: 0

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            horizontalAlignment: Text.AlignHCenter
            text: menuStore.currentTitle
            font.pixelSize: themeStore.fontHeading
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
        }

        Item { Layout.preferredHeight: 8 }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: menuList
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                topMargin: 4
                bottomMargin: 8
                spacing: 2
                clip: true
                model: menuStore.currentItems
                currentIndex: menuStore.selectedIndex
                highlightMoveDuration: 150

                delegate: MenuItem {
                    width: menuList.width
                    title: modelData.title
                    itemType: modelData.type
                    isSelected: index === menuStore.selectedIndex
                    currentValue: modelData.currentValue
                    hasChildren: modelData.hasChildren
                    leadingIcon: modelData.leadingIcon !== undefined ? modelData.leadingIcon : ""
                    valueLabel: modelData.valueLabel !== undefined ? modelData.valueLabel : ""
                    caution: modelData.caution !== undefined ? modelData.caution : false
                }

                // Keep the selected item clear of the 40 px gradient indicators
                // at the top and bottom. The gradients hide themselves once
                // there's no more content to indicate, so we must clamp the
                // resulting contentY to the natural [0, max] range — otherwise
                // the last item lands 40 px above the bottom edge with blank
                // space below it, and wrapping back to index 0 places the
                // first item 40 px below the top edge.
                onCurrentIndexChanged: {
                    var gradientHeight = 40
                    var item = menuList.itemAtIndex(currentIndex)
                    if (!item) {
                        positionViewAtIndex(currentIndex, ListView.Contain)
                        return
                    }
                    var itemTop = item.y - menuList.contentY
                    var itemBottom = itemTop + item.height
                    var viewHeight = menuList.height

                    var target = menuList.contentY
                    if (itemBottom > viewHeight - gradientHeight) {
                        target = item.y + item.height - viewHeight + gradientHeight
                    } else if (itemTop < gradientHeight) {
                        target = item.y - gradientHeight
                    }
                    var maxContentY = Math.max(0, menuList.contentHeight - viewHeight)
                    menuList.contentY = Math.max(0, Math.min(maxContentY, target))
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 40
                visible: menuList.contentY > 5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.8) }
                    GradientStop { position: 1.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                }

                Text {
                    anchors.centerIn: parent
                    text: MaterialIcon.iconKeyboardArrowUp
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontTitle
                    color: themeStore.isDark ? "#8AFFFFFF" : "#8A000000" // white54 / black54
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 40
                visible: menuList.contentY < (menuList.contentHeight - menuList.height - 5)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                    GradientStop { position: 1.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.8) }
                }

                Text {
                    anchors.centerIn: parent
                    text: MaterialIcon.iconKeyboardArrowDown
                    font.family: "Material Icons"
                    font.pixelSize: themeStore.fontTitle
                    color: themeStore.isDark ? "#8AFFFFFF" : "#8A000000" // white54 / black54
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: controlHints.height
            color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.3) : Qt.rgba(1, 1, 1, 0.3)

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: themeStore.isDark ? "#1AFFFFFF" : "#1F000000"
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                // Do not advertise scrolling for a single-entry level.
                leftTap: !menuStore.canScroll ? ""
                       : (typeof translations !== "undefined"
                          ? translations.controlScroll : "Scroll")
                // A hold exits at root; otherwise name the parent when it fits.
                leftHold: {
                    if (typeof translations === "undefined")
                        return menuStore.isRoot ? "Close" : "Back"
                    if (menuStore.isRoot)
                        return translations.controlClose
                    return menuStore.parentTitle === ""
                         ? translations.controlBack
                         : translations.controlBackTo.arg(menuStore.parentTitle)
                }
                leftHoldShort: typeof translations !== "undefined"
                               ? translations.controlBack : "Back"
                rightTap: typeof translations !== "undefined"
                          ? translations.controlSelect : "Select"
                // The label names the selected primary action, rather than a generic shortcut.
                rightHold: menuStore.selectedPrimaryLabel
            }
        }
    }
}
