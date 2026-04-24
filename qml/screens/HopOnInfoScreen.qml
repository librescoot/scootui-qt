import QtQuick
import QtQuick.Layouts
import "../widgets/status_bars"
import "../widgets/components"

Rectangle {
    id: hopOnInfoScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color accentColor: isDark ? "#FF9800" : "#E65100"

    function startLearn() {
        if (typeof screenStore !== "undefined")
            screenStore.closeHopOnInfo()
        if (typeof hopOnStore !== "undefined")
            hopOnStore.startLearning()
    }

    function cancelBack() {
        if (typeof screenStore !== "undefined")
            screenStore.closeHopOnInfo()
    }

    readonly property bool canScrollDown: flickable.contentHeight > flickable.height
                                           && flickable.contentY + flickable.height < flickable.contentHeight - 2
    readonly property bool canScrollUp: flickable.contentY > 2

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            if (hopOnInfoScreen.canScrollDown) {
                scrollAnim.to = Math.min(flickable.contentY + 100,
                                          flickable.contentHeight - flickable.height)
                scrollAnim.restart()
            } else {
                hopOnInfoScreen.cancelBack()
            }
        }
        function onLeftHold() {
            if (hopOnInfoScreen.canScrollUp) {
                scrollAnim.to = Math.max(flickable.contentY - 100, 0)
                scrollAnim.restart()
            }
        }
        function onRightTap() { hopOnInfoScreen.startLearn() }
    }

    Column {
        anchors.fill: parent

        TopStatusBar {
            id: topBar
            width: parent.width
            height: 40
        }

        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - topBar.height - footer.height
            contentHeight: bodyColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            NumberAnimation {
                id: scrollAnim
                target: flickable
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }

            Column {
                id: bodyColumn
                width: flickable.width
                spacing: 14

                Item { width: 1; height: 16 }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof translations !== "undefined"
                          ? translations.hopOnInfoTitle : "Hop On / Hop Off"
                    color: hopOnInfoScreen.textPrimary
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.hopOnInfoBody1 : ""
                    color: hopOnInfoScreen.textPrimary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: bodyColumn.width - 48
                    text: typeof translations !== "undefined"
                          ? translations.hopOnInfoBody2 : ""
                    color: hopOnInfoScreen.textSecondary
                    font.pixelSize: themeStore.fontBody
                    lineHeight: 1.3
                    lineHeightMode: Text.ProportionalHeight
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 8 }
            }
        }

        Rectangle {
            id: footer
            width: parent.width
            height: controlHints.height + 1
            color: hopOnInfoScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: hopOnInfoScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: hopOnInfoScreen.canScrollDown
                    ? (typeof translations !== "undefined" ? translations.controlScroll : "Scroll")
                    : (typeof translations !== "undefined" ? translations.controlBack : "Back")
                rightAction: typeof translations !== "undefined"
                             ? translations.updateModeStart : "Start"
            }
        }
    }
}
