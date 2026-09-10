import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: card
    property var entry: ({})
    property var queuedCounts: ({})
    property bool isDark: true
    property bool compact: false
    property bool secondary: false
    readonly property color ink: (entry.priority === 0 || entry.kind === "error") ? (isDark ? "#fff4f3" : "#8f1717")
                                : entry.priority === 2 ? (isDark ? "#ffe0a3" : "#6b4300")
                                : (isDark ? "#f3f6f8" : "#1e2930")
    readonly property color accent: (entry.priority === 0 || entry.kind === "error") ? (isDark ? "#ff827b" : "#bc2929")
                                   : entry.priority === 2 ? (isDark ? "#ffd075" : "#8a5800")
                                   : entry.kind === "success" ? (isDark ? "#82dca0" : "#237840")
                                   : entry.kind === "debug" ? (isDark ? "#b7bdc3" : "#59636c")
                                   : (isDark ? "#73c7ff" : "#1976b8")
    // The dock allocates space above the unchanged speed glyphs, not the dial box.
    property real maximumHeight: 156
    readonly property real textBudget: Math.max(20, maximumHeight - 8)
    readonly property int titlePixelSize: preferredTitle.height + (bodyText.visible ? preferredBody.height + 2 : 0) > textBudget ? 18 : 20
    readonly property real overflowDistance: Math.max(0, textColumn.height - viewport.height)
    readonly property bool scrolling: scrollAnimation.running
    readonly property string scrollKey: (entry.id || "") + "\u0000" + titleText.text + "\u0000" + bodyText.text
    implicitHeight: Math.min(maximumHeight, Math.max(compact ? 48 : 96, textColumn.height + 8, counts.height + 8))

    function resetScroll() {
        scrollAnimation.stop()
        textColumn.y = 0
        if (visible && overflowDistance > 0) scrollAnimation.start()
    }
    onScrollKeyChanged: Qt.callLater(resetScroll)
    onOverflowDistanceChanged: Qt.callLater(resetScroll)
    onVisibleChanged: Qt.callLater(resetScroll)
    Component.onCompleted: Qt.callLater(resetScroll)
    readonly property color tint: entry.priority === 0 || entry.kind === "error" ? (isDark ? "#451917" : "#ffe1de")
                                  : entry.priority === 2 ? (isDark ? "#3d2c0d" : "#fff0ce")
                                  : entry.kind === "success" ? (isDark ? "#153522" : "#ddf4e5")
                                  : entry.kind === "debug" ? (isDark ? "#202428" : "#edf0f2")
                                  : (isDark ? "#132e40" : "#dff0fc")
    color: Qt.rgba(tint.r, tint.g, tint.b, 0.8)
    border.width: 0
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        spacing: 4
        Text {
            objectName: "notificationIcon"
            Layout.preferredWidth: card.compact ? 40 : 56
            horizontalAlignment: Text.AlignHCenter
            text: card.entry.priority === 0 || card.entry.kind === "error" ? MaterialIcon.iconErrorOutline
                  : card.entry.priority === 2 ? MaterialIcon.iconWarningAmber
                  : card.entry.id === "map-update" ? MaterialIcon.iconUpdate
                  : card.entry.kind === "success" ? MaterialIcon.iconCheckCircleOutline
                  : card.entry.kind === "debug" ? MaterialIcon.iconBugReport
                  : MaterialIcon.iconInfoOutline
            font.family: "Material Icons"
            font.pixelSize: 28
            color: card.accent
        }
        Item {
            id: viewport
            objectName: "notificationTextViewport"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(textColumn.height, card.textBudget)
            clip: card.overflowDistance > 0
            Column {
                id: textColumn
                objectName: "notificationTextContent"
                width: parent.width
                spacing: 2
                BalancedText {
                    id: titleText
                    objectName: "notificationTitle"
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maxWidth: parent.width
                    text: card.entry.id === "map-coverage"
                          ? (typeof translations !== "undefined" ? translations.mapOutOfCoverage : "No map for current location")
                          : card.entry.title || "Notification"
                    color: card.ink
                    font.pixelSize: card.titlePixelSize
                    font.weight: Font.DemiBold
                    lineHeightMode: Text.FixedHeight
                    lineHeight: font.pixelSize + 2
                }
                Text {
                    id: bodyText
                    objectName: "notificationBody"
                    textFormat: Text.PlainText
                    width: parent.width
                    visible: !!card.entry.body
                    text: card.entry.body || ""
                    color: card.isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                    font.pixelSize: 18
                    wrapMode: Text.Wrap
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 20
                }
            }
            Text {
                id: preferredTitle
                visible: false
                width: parent.width
                text: titleText.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: 20
                font.weight: Font.DemiBold
                lineHeightMode: Text.FixedHeight
                lineHeight: 22
            }
            Text {
                id: preferredBody
                visible: false
                width: parent.width
                text: bodyText.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font: bodyText.font
                lineHeightMode: Text.FixedHeight
                lineHeight: 20
            }
        }
        QueuedNotificationCounts {
            id: counts
            Layout.preferredWidth: implicitWidth
            Layout.minimumWidth: Layout.preferredWidth
            Layout.maximumWidth: Layout.preferredWidth
            queuedCounts: card.queuedCounts
            isDark: card.isDark
        }
    }
    // Move the complete title/body, never their layout or the surrounding card.
    SequentialAnimation {
        id: scrollAnimation
        loops: Animation.Infinite
        PauseAnimation { duration: 1200 }
        NumberAnimation {
            target: textColumn
            property: "y"
            from: 0
            to: -card.overflowDistance
            duration: Math.ceil(card.overflowDistance / 20 * 1000)
        }
        PauseAnimation { duration: 1600 }
        PropertyAction { target: textColumn; property: "y"; value: 0 }
    }
    Rectangle {
        visible: card.overflowDistance > 0
        x: viewport.x + viewport.width + 1
        y: viewport.y + (card.overflowDistance > 0 ? -textColumn.y / card.overflowDistance : 0) * (viewport.height - height)
        width: 2
        height: Math.max(8, viewport.height * viewport.height / textColumn.height)
        color: card.accent
    }
}
