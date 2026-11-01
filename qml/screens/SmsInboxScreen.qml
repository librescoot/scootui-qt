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
    property bool confirming: false
    property bool confirmDelete: false
    property bool deleting: false

    function close() {
        screenStore.closeSmsInbox()
        // Resume after this brake signal has finished dispatching.
        Qt.callLater(function() { menuStore.resume() })
    }
    function scrollBy(delta) {
        scrollAnimation.to = Math.max(0, Math.min(bodyScroll.contentY + delta,
                                                 bodyScroll.contentHeight - bodyScroll.height))
        scrollAnimation.restart()
    }
    Connections {
        target: inputHandler
        function onLeftTap() {
            if (root.confirming) root.confirmDelete = !root.confirmDelete
            else root.scrollBy(120)
        }
        function onRightHold() { if (!root.confirming) root.scrollBy(-120) }
        function onRightTap() {
            if (root.deleting) return
            if (!root.confirming) {
                if (smsStore.connected && smsStore.openedAvailable) {
                    root.confirmDelete = false
                    root.confirming = true
                }
            } else if (!root.confirmDelete) root.confirming = false
            else if (smsStore.deleteOpenedMessage()) {
                root.deleting = true
                Qt.callLater(root.close)
            }
            else root.confirming = false
        }
        function onLeftHold() {
            if (root.confirming) root.confirming = false
            else root.close()
        }
    }
    Rectangle {
        anchors.fill: parent
        z: 2
        visible: root.confirming
        color: root.color
        Column {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: 24
            spacing: 20
            Text {
                width: parent.width
                text: translations.smsDeleteConfirm
                color: root.foreground
                font.pixelSize: themeStore.fontHeading
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: root.message.from || translations.smsUnknownSender
                textFormat: Text.PlainText
                color: root.muted
                font.pixelSize: themeStore.fontBody
                elide: Text.ElideRight
            }
            Repeater {
                model: [translations.controlCancel, translations.smsDelete]
                Rectangle {
                    required property int index
                    required property string modelData
                    width: parent.width; height: 56; radius: 8
                    color: (index === 1) === root.confirmDelete ? (themeStore.isDark ? "#3DFFFFFF" : "#1F000000") : "transparent"
                    Text { anchors.centerIn: parent; text: modelData; color: root.foreground; font.pixelSize: themeStore.fontTitle }
                }
            }
        }
        ControlHints {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            reservedRows: 2
            leftTap: translations.controlNext
            leftHold: translations.controlBack
            rightTap: root.confirmDelete ? translations.smsDelete : translations.controlCancel
            isDark: themeStore.isDark
        }
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
                    width: parent.width
                    visible: !smsStore.connected || !smsStore.openedAvailable
                    text: !smsStore.connected ? translations.smsDisconnected : translations.smsUnavailable
                    color: root.muted
                    font.pixelSize: themeStore.fontCaption
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }
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
                rightTap: smsStore.connected && smsStore.openedAvailable ? translations.smsDelete : ""
            }
        }
    }
}
