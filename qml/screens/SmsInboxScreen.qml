import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: root
    color: themeStore.isDark ? "black" : "white"
    readonly property color foreground: themeStore.isDark ? "white" : "black"
    readonly property color muted: themeStore.isDark ? "#99FFFFFF" : "#8A000000"
    readonly property var message: smsStore.openedMessage
    readonly property bool canScrollDown: bodyScroll.contentY + bodyScroll.height < bodyScroll.contentHeight - 2
    readonly property bool canScrollUp: bodyScroll.contentY > 2

    function close() {
        screenStore.closeSmsInbox()
        menuStore.resume()
    }
    function scrollBy(delta) {
        scrollAnimation.to = Math.max(0, Math.min(bodyScroll.contentY + delta,
                                                 bodyScroll.contentHeight - bodyScroll.height))
        scrollAnimation.restart()
    }
    Connections {
        target: inputHandler
        function onLeftTap() { root.scrollBy(120) }
        function onRightHold() { root.scrollBy(-120) }
        function onLeftHold() { root.close() }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 24
        spacing: 12

        Text {
            objectName: "smsReaderTitle"
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            text: translations.smsMessage
            color: root.foreground
            font.pixelSize: themeStore.fontHeading
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
        }
        Flickable {
            id: bodyScroll
            objectName: "smsBodyScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            clip: true
            contentWidth: width
            contentHeight: bodyColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            NumberAnimation {
                id: scrollAnimation
                target: bodyScroll
                property: "contentY"
                duration: 200
                easing.type: Easing.OutCubic
            }
            Column {
                id: bodyColumn
                width: bodyScroll.width
                spacing: 12
                Text {
                    objectName: "smsSender"
                    width: parent.width
                    text: root.message.from || translations.smsUnknownSender
                    color: root.foreground
                    font.pixelSize: themeStore.fontTitle
                    font.weight: Font.Bold
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
                Text {
                    objectName: "smsTimestamp"
                    width: parent.width
                    readonly property var timestamp: new Date(root.message.timestamp || "")
                    text: isNaN(timestamp.getTime()) ? (root.message.timestamp || "")
                        : timestamp.toLocaleString(Qt.locale(translations.language), Locale.ShortFormat)
                    visible: text !== ""
                    color: root.muted
                    font.pixelSize: themeStore.fontCaption
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
                Text {
                    objectName: "smsBody"
                    width: parent.width
                    text: root.message.text || ""
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                }
                Item { width: 1; height: 8 }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: controlHints.height + 1
            color: "transparent"
            Rectangle {
                width: parent.width
                height: 1
                color: themeStore.isDark ? "#1AFFFFFF" : "#1F000000"
            }
            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                reservedRows: 2
                leftTap: root.canScrollDown ? translations.controlScroll : ""
                leftHold: translations.controlBack
                rightHold: root.canScrollUp ? translations.controlScrollUp : ""
            }
        }
    }
}
