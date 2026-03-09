import QtQuick

Rectangle {
    id: menuItem

    property string title: ""
    property string itemType: "action"
    property bool isSelected: false
    property int currentValue: 0
    property bool hasChildren: false

    height: 54
    color: isSelected
           ? (themeStore.isDark ? "#3DFFFFFF" : "#1F000000")
           : "transparent"
    radius: 8

    Row {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 8

        // Title
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 40
            text: menuItem.title
            font.pixelSize: 20
            font.bold: isSelected
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
            elide: Text.ElideRight
        }

        // Trailing icon
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            horizontalAlignment: Text.AlignRight
            text: {
                if (itemType === "submenu" || hasChildren)
                    return "\u203A"
                if (itemType === "setting" && currentValue === 1)
                    return "\u2713"
                return ""
            }
            font.pixelSize: itemType === "submenu" || hasChildren ? 24 : 20
            font.bold: itemType === "setting" && currentValue === 1
            color: {
                if (itemType === "setting" && currentValue === 1)
                    return "#4CAF50"
                return themeStore.isDark ? "#99FFFFFF" : "#99000000"
            }
        }
    }
}
