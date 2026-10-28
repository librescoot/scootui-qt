import QtQuick

Rectangle {
    id: root
    color: themeStore.isDark ? "black" : "white"
    readonly property color foreground: themeStore.isDark ? "white" : "black"
    readonly property color muted: themeStore.isDark ? "#aaaaaa" : "#555555"
    property int selected: 0
    property var opened: null
    property bool confirmingDelete: false
    readonly property var messages: smsStore.messages

    function close() {
        if (confirmingDelete) { confirmingDelete = false; return }
        if (opened !== null) { opened = null; return }
        screenStore.closeSmsInbox()
        menuStore.resume()
    }
    function openSelected() {
        if (selected < 0 || selected >= messages.length) return
        opened = messages[selected]
        smsStore.markRead(opened.id)
        bodyScroll.contentY = 0
    }
    Connections {
        target: inputHandler
        function onLeftTap() {
            if (root.opened !== null) {
                bodyScroll.contentY = Math.min(bodyScroll.contentY + 120,
                    Math.max(0, bodyScroll.contentHeight - bodyScroll.height))
            } else if (root.messages.length > 0) {
                root.selected = (root.selected + 1) % root.messages.length
                list.positionViewAtIndex(root.selected, ListView.Contain)
            }
        }
        function onRightHold() {
            if (root.opened !== null) bodyScroll.contentY = Math.max(0, bodyScroll.contentY - 120)
            else if (root.messages.length > 0) {
                root.selected = (root.selected + root.messages.length - 1) % root.messages.length
                list.positionViewAtIndex(root.selected, ListView.Contain)
            }
        }
        function onRightTap() {
            if (root.opened === null) root.openSelected()
            else if (!smsStore.connected) return
            else if (!root.confirmingDelete) root.confirmingDelete = true
            else {
                smsStore.deleteMessage(root.opened.id)
                root.confirmingDelete = false
                root.opened = null
                root.selected = 0
            }
        }
        function onLeftHold() { root.close() }
        function onLeftDoubleTap() {
            if (root.opened === null && smsStore.forwardTo !== "" && smsStore.forwardTo !== "off")
                smsStore.disableForwarding()
        }
    }
    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 8
        Text {
            width: parent.width
            text: root.opened === null ? qsTr("Messages") : (root.opened.from || qsTr("Unknown sender"))
            color: root.foreground
            font.pixelSize: themeStore.fontBody + 6
            font.bold: true
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
        Text {
            visible: root.opened === null && smsStore.available
            width: parent.width
            text: !smsStore.connected ? qsTr("Disconnected: cached messages")
                : smsStore.forwardTo && smsStore.forwardTo !== "off"
                    ? qsTr("Forwarding to %1 (double left to disable)").arg(smsStore.forwardTo)
                    : qsTr("Forwarding off (configure in app)")
            color: root.muted
            font.pixelSize: themeStore.fontCaption
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
        Text {
            visible: root.opened !== null
            width: parent.width
            text: root.opened !== null ? (root.opened.timestamp || "") : ""
            color: root.muted
            font.pixelSize: themeStore.fontCaption
            textFormat: Text.PlainText
        }
        Item {
            width: parent.width
            height: parent.height - 110
            ListView {
                id: list
                anchors.fill: parent
                clip: true
                visible: root.opened === null && smsStore.available
                model: root.messages
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: list.width
                    height: 66
                    color: index === root.selected ? (themeStore.isDark ? "#303030" : "#dddddd") : "transparent"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 5
                        Text {
                            width: parent.width
                            text: (modelData.read ? "" : "● ") + (modelData.from || qsTr("Unknown sender"))
                            color: root.foreground
                            font.pixelSize: themeStore.fontBody
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: modelData.text || ""
                            color: root.muted
                            font.pixelSize: themeStore.fontCaption
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: root.opened === null && (!smsStore.available || root.messages.length === 0)
                text: smsStore.available ? qsTr("No messages") : qsTr("Messages unavailable")
                color: root.muted
                font.pixelSize: themeStore.fontBody
            }
            Flickable {
                id: bodyScroll
                anchors.fill: parent
                visible: root.opened !== null
                clip: true
                contentWidth: width
                contentHeight: bodyText.height
                boundsBehavior: Flickable.StopAtBounds
                Text {
                    id: bodyText
                    width: bodyScroll.width
                    text: root.opened !== null ? (root.opened.text || "") : ""
                    color: root.foreground
                    font.pixelSize: themeStore.fontBody
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }
        }
        Text {
            width: parent.width
            text: root.opened === null ? qsTr("Left: next   Right: open   Hold left: back")
                 : root.confirmingDelete ? qsTr("Right: confirm delete   Hold left: cancel")
                 : smsStore.connected ? qsTr("Left: scroll   Right: delete   Hold left: back")
                 : qsTr("Left: scroll   Hold left: back")
            color: root.muted
            font.pixelSize: themeStore.fontCaption
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }
    }
}
