import QtQuick

Rectangle {
    id: menuItem

    property string title: ""
    property string itemType: "action"
    property bool isSelected: false
    property int currentValue: 0
    property bool hasChildren: false
    property string leadingIcon: ""
    property string valueLabel: ""

    // Flutter: Container is 50px (54 total slot - 4px from Padding(vertical:2))
    // ListView spacing: 4 handles the inter-item gap
    height: 50
    color: isSelected
           ? (themeStore.isDark ? "#3DFFFFFF" : "#1F000000")
           : "transparent"
    radius: themeStore.radiusCard

    Row {
        anchors.fill: parent
        // Flutter: Container padding symmetric(horizontal: 16, vertical: 8)
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 8

        // Leading icon (Flutter: optional Icon before title, size 20, white70/black54)
        Text {
            id: leadingIconText
            anchors.verticalCenter: parent.verticalCenter
            visible: menuItem.leadingIcon !== ""
            text: menuItem.leadingIcon
            font.family: "Material Icons"
            font.pixelSize: themeStore.fontTitle
            color: themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
        }

        // Title
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
                   - (trailingIcon.visible ? trailingIcon.width + parent.spacing : 0)
                   - (trailingValue.visible ? trailingValue.implicitWidth + parent.spacing : 0)
                   - (leadingIconText.visible ? leadingIconText.width + parent.spacing : 0)
            text: menuItem.title
            font.pixelSize: themeStore.fontTitle
            font.weight: isSelected ? Font.Bold : Font.Normal
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
            elide: isSelected ? Text.ElideNone : Text.ElideRight
            wrapMode: isSelected ? Text.WordWrap : Text.NoWrap
            maximumLineCount: isSelected ? 100 : 1
        }

        // Trailing icon (submenu chevron / setting check) — hidden for cycle type
        Text {
            id: trailingIcon
            anchors.verticalCenter: parent.verticalCenter
            visible: itemType !== "cycle"
            width: visible ? 24 : 0
            horizontalAlignment: Text.AlignRight
            text: {
                if (itemType === "submenu" || hasChildren)
                    return "\ue15f" // chevron_right
                if (itemType === "setting" && currentValue === 1)
                    return "\ue156" // check
                return ""
            }
            font.family: "Material Icons"
            // Flutter: check icon size 20, chevron_right default size 24
            font.pixelSize: (itemType === "setting" && currentValue === 1) ? 20 : 24
            color: {
                // Flutter: check uses text color, chevron uses white70/black54
                if (itemType === "setting" && currentValue === 1)
                    return themeStore.isDark ? "#FFFFFF" : "#000000"
                return themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
            }
        }

        // Trailing value label for inline cycle settings
        Text {
            id: trailingValue
            anchors.verticalCenter: parent.verticalCenter
            visible: itemType === "cycle"
            text: menuItem.valueLabel
            font.pixelSize: themeStore.fontBody
            font.weight: Font.Normal
            color: themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
            horizontalAlignment: Text.AlignRight
        }
    }
}
