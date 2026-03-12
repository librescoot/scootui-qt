import QtQuick
import QtQuick.Layouts

Item {
    id: menuOverlay
    anchors.fill: parent
    visible: opacity > 0
    opacity: menuStore.isOpen ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    // Background
    Rectangle {
        anchors.fill: parent
        color: themeStore.isDark ? "#000000" : "#FFFFFF"
        opacity: 0.9
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Title bar
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            Text {
                anchors.centerIn: parent
                text: menuStore.currentTitle
                font.pixelSize: 24
                font.bold: true
                color: themeStore.isDark ? "#FFFFFF" : "#000000"
            }
        }

        // Menu items list
        ListView {
            id: menuList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
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
            }

            // Ensure selected item is visible
            onCurrentIndexChanged: {
                positionViewAtIndex(currentIndex, ListView.Contain)
            }
        }

        // Bottom control hints
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "transparent"

            // Top border
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                height: 1
                color: themeStore.isDark ? "#33FFFFFF" : "#33000000"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24

                Row {
                    spacing: 4
                    Text {
                        text: "\ue092" // arrow_back
                        font.family: "Material Icons"
                        font.pixelSize: 18
                        color: themeStore.isDark ? "#99FFFFFF" : "#99000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: typeof translations !== "undefined" ? translations.controlBack : "Next"
                        font.pixelSize: 16
                        color: themeStore.isDark ? "#99FFFFFF" : "#99000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Item { Layout.fillWidth: true }

                Row {
                    spacing: 4
                    Text {
                        text: typeof translations !== "undefined" ? translations.controlSelect : "Select"
                        font.pixelSize: 16
                        color: themeStore.isDark ? "#99FFFFFF" : "#99000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "\ue09b" // arrow_forward
                        font.family: "Material Icons"
                        font.pixelSize: 18
                        color: themeStore.isDark ? "#99FFFFFF" : "#99000000"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
