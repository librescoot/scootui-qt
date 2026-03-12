import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Item {
    id: menuOverlay
    anchors.fill: parent
    visible: opacity > 0
    opacity: menuStore.isOpen ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // Background (covers full screen including status bar area)
    Rectangle {
        anchors.fill: parent
        color: themeStore.isDark ? "#000000" : "#FFFFFF"
        opacity: 0.9
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 40   // Leave space for top status bar (Flutter: padding top: 40)
        spacing: 0

        // 8px gap above title (Flutter: SizedBox(height: 8))
        Item { Layout.preferredHeight: 8 }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: menuStore.currentTitle
            font.pixelSize: 24
            font.bold: true
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
        }

        // 16px gap below title (Flutter: SizedBox(height: 16))
        Item { Layout.preferredHeight: 16 }

        // Menu items list with scroll indicators (Flutter: Stack with ListView + gradient overlays)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: menuList
                anchors.fill: parent
                anchors.leftMargin: 40   // Flutter: ListView padding left: 40
                anchors.rightMargin: 40  // Flutter: ListView padding right: 40
                topMargin: 12           // Flutter: ListView padding top: 12
                bottomMargin: 12        // Flutter: ListView padding bottom: 12
                spacing: 4              // Flutter: Padding(vertical: 2) per item = 4px between
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
                }

                // Ensure selected item is visible
                onCurrentIndexChanged: {
                    positionViewAtIndex(currentIndex, ListView.Contain)
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
                    GradientStop { position: 0.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                    GradientStop { position: 1.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.0) : Qt.rgba(1, 1, 1, 0.0) }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\ue356" // keyboard_arrow_up
                    font.family: "Material Icons"
                    font.pixelSize: 24
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
                    GradientStop { position: 1.0; color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(1, 1, 1, 0.9) }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\ue353" // keyboard_arrow_down
                    font.family: "Material Icons"
                    font.pixelSize: 24
                    color: themeStore.isDark ? "#8AFFFFFF" : "#8A000000" // white54 / black54
                }
            }
        }

        // Bottom control hints (Flutter: Container with padding h:8 v:8, semi-transparent bg)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: controlHints.height + 16  // 8px padding top + bottom
            color: themeStore.isDark ? Qt.rgba(0, 0, 0, 0.3) : Qt.rgba(1, 1, 1, 0.3)

            // Top border (Flutter: Colors.white10 / Colors.black12)
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
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                leftAction: typeof translations !== "undefined"
                            ? translations.controlNextItem : "Next Item"
                rightAction: typeof translations !== "undefined"
                             ? translations.controlSelect : "Select"
            }
        }
    }
}
