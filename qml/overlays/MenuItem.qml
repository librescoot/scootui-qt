import QtQuick
import "../widgets/components"

Rectangle {
    id: menuItem

    property string title: ""
    property string itemType: "action"
    property bool isSelected: false
    property int currentValue: 0
    property bool hasChildren: false
    property string leadingIcon: ""
    property string valueLabel: ""
    property bool caution: false

    // Selected labels may wrap, so grow the row instead of overlapping its successor.
    height: Math.max(50, titleText.implicitHeight + 16)
    color: isSelected
           ? (themeStore.isDark ? "#3DFFFFFF" : "#1F000000")
           : "transparent"
    radius: themeStore.radiusCard

    Row {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 8

        Text {
            id: cautionIcon
            anchors.verticalCenter: parent.verticalCenter
            visible: menuItem.caution
            text: MaterialIcon.iconWarningAmber
            font.family: "Material Icons"
            font.pixelSize: 18
            color: themeStore.statusWarning
        }

        Text {
            id: leadingIconText
            anchors.verticalCenter: parent.verticalCenter
            visible: menuItem.leadingIcon !== ""
            text: menuItem.leadingIcon
            font.family: "Material Icons"
            font.pixelSize: themeStore.fontTitle
            color: themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
        }

        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
                   - (trailingIcon.visible ? trailingIcon.width + parent.spacing : 0)
                   - (trailingValue.visible ? trailingValue.implicitWidth + parent.spacing : 0)
                   - (leadingIconText.visible ? leadingIconText.width + parent.spacing : 0)
                   - (cautionIcon.visible ? cautionIcon.width + parent.spacing : 0)
            text: menuItem.title
            font.pixelSize: themeStore.fontTitle
            font.weight: isSelected ? Font.Bold : Font.Normal
            color: themeStore.isDark ? "#FFFFFF" : "#000000"
            // Bound geocoded saved-location labels while keeping a selected row readable.
            elide: Text.ElideRight
            wrapMode: isSelected ? Text.WordWrap : Text.NoWrap
            maximumLineCount: isSelected ? 3 : 1
        }

        Text {
            id: trailingValue
            anchors.verticalCenter: parent.verticalCenter
            visible: menuItem.valueLabel !== ""
            text: menuItem.valueLabel
            font.pixelSize: themeStore.fontBody
            font.weight: Font.Normal
            color: themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
            horizontalAlignment: Text.AlignRight
        }

        Text {
            id: trailingIcon
            // Visibility follows the glyph so empty affordances consume no row width.
            readonly property string glyph: {
                if (itemType === "cycle")
                    return ""
                if (itemType === "submenu" || hasChildren)
                    return MaterialIcon.iconChevronRight
                if (itemType === "setting" && currentValue === 1)
                    return MaterialIcon.iconCheck
                return ""
            }
            anchors.verticalCenter: parent.verticalCenter
            visible: glyph !== ""
            width: visible ? 24 : 0
            horizontalAlignment: Text.AlignRight
            text: glyph
            font.family: "Material Icons"
            font.pixelSize: (itemType === "setting" && currentValue === 1) ? 20 : 24
            color: {
                if (itemType === "setting" && currentValue === 1)
                    return themeStore.isDark ? "#FFFFFF" : "#000000"
                return themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
            }
        }
    }
}
