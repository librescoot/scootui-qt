import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Item {
    id: menuOverlay
    anchors.fill: parent
    visible: opacity > 0
    opacity: menuController.isOpen ? 1.0 : 0.0

    property Item blurSource

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // Frosted glass background
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
        enabled: menuController.isOpen
        function onLeftTap()  { menuController.navigateDown() }
        function onLeftHold() { menuController.goBack()        }
        function onRightTap() { menuController.selectItem()   }
        // Right long-tap runs the selected row's primary action without
        // entering it. Only ever bound to something cheap to undo: at 800 ms
        // a deliberate but slow select crosses the same threshold.
        function onRightHold() { menuController.activatePrimary() }
        // The 3 s hold leaves the menu from any depth, deliberately without a
        // row in the bar: an escape from four levels down is worth having and
        // is not worth 19 px of every screen to advertise. It arrives after
        // the 800 ms long-tap has already popped one level, so from the root
        // the menu has closed before this can fire.
        function onLeftBrakeHold() { menuController.close() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 24   // Leave space for top status bar
        spacing: 0

        // Title. A saved location's submenu takes the location's own label as
        // its header, and that label is a geocoded address, so the header has
        // to be bounded: unconstrained it ran off both edges of the panel. The
        // full address is readable on the row that opens it, which wraps.
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            horizontalAlignment: Text.AlignHCenter
            text: menuController.currentTitle
            font.pixelSize: themeStore.fontHeading
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
        }

        Item { Layout.preferredHeight: 8 }

        // Menu items list with scroll indicators (Flutter: Stack with ListView + gradient overlays)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: menuList
                anchors.fill: parent
                anchors.leftMargin: 40   // Flutter: ListView padding left: 40
                anchors.rightMargin: 40  // Flutter: ListView padding right: 40
                topMargin: 4
                bottomMargin: 8
                spacing: 2
                clip: true
                model: menuController.currentItems
                currentIndex: menuController.selectedIndex
                highlightMoveDuration: 150

                delegate: MenuItem {
                    width: menuList.width
                    title: modelData.title
                    itemType: modelData.type
                    isSelected: index === menuController.selectedIndex
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

            // Top scroll indicator (Flutter: gradient fade + keyboard_arrow_up)
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

            // Bottom scroll indicator (Flutter: gradient fade + keyboard_arrow_down)
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

        // Bottom control hints
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: controlHints.height
            color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.3) : Qt.rgba(1, 1, 1, 0.3)

            // Top border
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
                // A level with one entry has nothing to scroll through.
                leftTap: !menuController.canScroll ? ""
                       : (typeof translations !== "undefined"
                          ? translations.controlScroll : "Scroll")
                // At the root the hold leaves the menu rather than going up a
                // level, so it says so.
                leftHold: {
                    if (typeof translations === "undefined")
                        return menuController.isRoot ? "Close" : "Back"
                    if (menuController.isRoot)
                        return translations.controlClose
                    // Name the level it lands on. The hold row has the bar to
                    // itself (nothing binds a right hold), so even the longest
                    // German level name fits.
                    return menuController.parentTitle === ""
                         ? translations.controlBack
                         : translations.controlBackTo.arg(menuController.parentTitle)
                }
                // Plain "Back" for when naming the level would collide with
                // the shortcut on the same row.
                leftHoldShort: typeof translations !== "undefined"
                               ? translations.controlBack : "Back"
                rightTap: typeof translations !== "undefined"
                          ? translations.controlSelect : "Select"
                // Named by the row it would run, so the shortcut says what it
                // does rather than that it exists. Empty on rows that declare
                // no primary action, which is most of them.
                rightHold: menuController.selectedPrimaryLabel
            }
        }
    }
}
