import QtQuick

Item {
    id: controlHints

    property string leftAction: ""
    property string rightAction: ""

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color secondaryColor: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color primaryColor: isDark ? "#FFFFFF" : "#000000"
    readonly property color borderColor: isDark ? "#33FFFFFF" : "#1F000000"

    height: 32
    implicitHeight: 32

    // Left brake hint
    Rectangle {
        id: leftHintBg
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.leftAction !== ""
        width: leftHint.width + 16
        height: leftHint.height + 8
        color: "transparent"
        border.width: 1
        border.color: controlHints.borderColor
        radius: 4
        // Open left edge (extends past screen)
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: controlHints.isDark ? "#000000" : "#FFFFFF"
        }

        Column {
            id: leftHint
            anchors.centerIn: parent
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
    }

    // Right brake hint
    Rectangle {
        id: rightHintBg
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: controlHints.rightAction !== ""
        width: rightHint.width + 16
        height: rightHint.height + 8
        color: "transparent"
        border.width: 1
        border.color: controlHints.borderColor
        radius: 4
        // Open right edge (extends past screen)
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: controlHints.isDark ? "#000000" : "#FFFFFF"
        }

        Column {
            id: rightHint
            anchors.centerIn: parent
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
}
