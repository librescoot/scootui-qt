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
    // Note under the title, revealed only while the row is selected: it is
    // guidance for the rider about to act on this entry, not a permanent
    // second line on every row.
    property string subtitle: ""

    readonly property bool showSubtitle: isSelected && menuItem.subtitle !== ""

    // 50px base slot (ListView spacing: 4 handles the inter-item gap). When
    // selected, the title switches to WordWrap and may span several lines —
    // grow the row so it doesn't overlap the next item. 16 = Row top+bottom margin.
    height: Math.max(50, textColumn.implicitHeight + 16)
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

        // Title, plus the selected row's note underneath it
        Column {
            id: textColumn
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
                   - (trailingIcon.visible ? trailingIcon.width + parent.spacing : 0)
                   - (trailingValue.visible ? trailingValue.implicitWidth + parent.spacing : 0)
                   - (leadingIconText.visible ? leadingIconText.width + parent.spacing : 0)
            spacing: menuItem.showSubtitle ? 2 : 0

            Text {
                id: titleText
                width: parent.width
                text: menuItem.title
                font.pixelSize: themeStore.fontTitle
                font.weight: isSelected ? Font.Bold : Font.Normal
                color: themeStore.isDark ? "#FFFFFF" : "#000000"
                elide: isSelected ? Text.ElideNone : Text.ElideRight
                wrapMode: isSelected ? Text.WordWrap : Text.NoWrap
                maximumLineCount: isSelected ? 100 : 1
            }

            Text {
                width: parent.width
                visible: menuItem.showSubtitle
                text: menuItem.subtitle
                font.pixelSize: themeStore.fontBody
                color: themeStore.isDark ? "#B3FFFFFF" : "#8A000000"
                wrapMode: Text.WordWrap
            }
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
                    return MaterialIcon.iconChevronRight
                if (itemType === "setting" && currentValue === 1)
                    return MaterialIcon.iconCheck
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

        // Trailing value label: the current option on inline cycle settings, or
        // any state an action wants to report next to what it does.
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
    }
}
