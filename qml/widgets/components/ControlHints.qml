import QtQuick

Item {
    id: controlHints

    property string leftAction: ""
    property string rightAction: ""
    property string leftLabel: ""

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color secondaryColor: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color primaryColor: isDark ? "#FFFFFF" : "#000000"
    readonly property color hintBg: isDark ? "#1AFFFFFF" : "#0F000000"

    height: 40
    implicitHeight: 40

    // Left brake hint
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.leftAction !== ""
        width: leftHint.width + 24
        height: leftHint.height + 12
        color: "transparent"
        radius: themeStore.radiusCard
        // Extend past left screen edge
        anchors.leftMargin: -6

        Column {
            id: leftHint
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 3
            spacing: 2

            Text {
                text: controlHints.leftLabel !== "" ? controlHints.leftLabel
                    : (typeof translations !== "undefined" ? translations.controlLeftBrake : "Left Brake")
                color: controlHints.secondaryColor
                font.pixelSize: themeStore.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.5
            }

            Text {
                text: controlHints.leftAction
                color: controlHints.primaryColor
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
            }
        }
    }

    // Right brake hint
    Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.rightAction !== ""
        width: rightHint.width + 24
        height: rightHint.height + 12
        color: "transparent"
        radius: themeStore.radiusCard
        // Extend past right screen edge
        anchors.rightMargin: -6

        Column {
            id: rightHint
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: -3
            spacing: 2

            Text {
                anchors.right: parent.right
                text: typeof translations !== "undefined" ? translations.controlRightBrake : "Right Brake"
                color: controlHints.secondaryColor
                font.pixelSize: themeStore.fontMicro
                font.weight: Font.Medium
                font.letterSpacing: 0.5
            }

            Text {
                anchors.right: parent.right
                text: controlHints.rightAction
                color: controlHints.primaryColor
                font.pixelSize: themeStore.fontBody
                font.weight: Font.Bold
            }
        }
    }
}
