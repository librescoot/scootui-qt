import QtQuick

Item {
    id: controlHints

    property string leftAction: ""
    property string rightAction: ""

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color secondaryColor: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color primaryColor: isDark ? "#FFFFFF" : "#000000"

    height: 32
    implicitHeight: 32

    // Left brake hint
    Column {
        id: leftHint
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.leftAction !== ""
        spacing: 2

        Text {
            text: typeof translations !== "undefined" ? translations.controlLeftBrake : "Left Brake"
            color: controlHints.secondaryColor
            font.pixelSize: 10
            font.weight: Font.Medium
            font.letterSpacing: 0.5
        }

        Text {
            text: controlHints.leftAction
            color: controlHints.primaryColor
            font.pixelSize: 14
            font.bold: true
        }
    }

    // Right brake hint
    Column {
        id: rightHint
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.rightAction !== ""
        spacing: 2

        Text {
            horizontalAlignment: Text.AlignRight
            text: typeof translations !== "undefined" ? translations.controlRightBrake : "Right Brake"
            color: controlHints.secondaryColor
            font.pixelSize: 10
            font.weight: Font.Medium
            font.letterSpacing: 0.5
        }

        Text {
            horizontalAlignment: Text.AlignRight
            text: controlHints.rightAction
            color: controlHints.primaryColor
            font.pixelSize: 14
            font.bold: true
        }
    }
}
