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
    // Entry whose default is almost always right: Update Type and Release
    // Channel, both of which cost a full-image download to get wrong.
    property bool caution: false

    // 50px base slot (ListView spacing: 4 handles the inter-item gap). When
    // selected, the title switches to WordWrap and may span several lines —
    // grow the row so it doesn't overlap the next item. 16 = Row top+bottom margin.
    height: Math.max(50, titleText.implicitHeight + 16)
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

        // Caution marker. Sits ahead of the title so the marked rows read as a
        // column down the left rather than something to hunt for at the end.
        Text {
            id: cautionIcon
            anchors.verticalCenter: parent.verticalCenter
            visible: menuItem.caution
            text: MaterialIcon.iconWarningAmber
            font.family: "Material Icons"
            font.pixelSize: 18
            color: themeStore.statusWarning
        }

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
            elide: isSelected ? Text.ElideNone : Text.ElideRight
            wrapMode: isSelected ? Text.WordWrap : Text.NoWrap
            maximumLineCount: isSelected ? 100 : 1
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

        // Trailing icon (submenu chevron / setting check) — hidden for cycle
        // type. Last in the row: a row can carry both a value and a chevron
        // (Updates > Change Update Type shows "Delta >"), and the chevron is
        // the affordance, so it belongs at the edge rather than between the
        // title and the state it describes.
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
    }
}
