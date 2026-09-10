import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: card
    property var entry: ({})
    property var queuedCounts: ({})
    property bool isDark: true
    property bool compact: false
    readonly property color ink: entry.priority === 0 ? (isDark ? "#fff4f3" : "#8f1717")
                                : entry.priority === 2 ? (isDark ? "#ffe0a3" : "#6b4300")
                                : (isDark ? "#f3f6f8" : "#1e2930")
    readonly property color accent: entry.priority === 0 ? (isDark ? "#ff827b" : "#bc2929")
                                   : entry.priority === 2 ? (isDark ? "#ffd075" : "#8a5800")
                                   : entry.kind === "success" ? (isDark ? "#82dca0" : "#237840")
                                   : entry.kind === "debug" ? (isDark ? "#b7bdc3" : "#59636c")
                                   : (isDark ? "#73c7ff" : "#1976b8")
    // Two slots must stay above the unchanged speed glyphs, even with long text.
    implicitHeight: Math.max(compact ? 48 : 96, content.implicitHeight + (compact ? 12 : 24))
    color: isDark ? Qt.rgba(0, 0, 0, 0.8) : Qt.rgba(1, 1, 1, 0.8)
    border.width: 0
    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 8
        spacing: 8
        Text {
            objectName: "notificationIcon"
            Layout.preferredWidth: card.compact ? 48 : 80
            horizontalAlignment: Text.AlignHCenter
            text: card.entry.priority === 0 || card.entry.kind === "error" ? MaterialIcon.iconErrorOutline
                  : card.entry.priority === 2 ? MaterialIcon.iconWarningAmber
                  : card.entry.id === "map-update" ? MaterialIcon.iconUpdate
                  : card.entry.kind === "success" ? MaterialIcon.iconCheckCircleOutline
                  : MaterialIcon.iconNavigation
            font.family: "Material Icons"
            font.pixelSize: 28
            color: card.accent
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Item {
                Layout.fillWidth: true
                implicitHeight: titleText.height
                BalancedText {
                    id: titleText
                    objectName: "notificationTitle"
                    textFormat: Text.PlainText
                    maxWidth: parent.width
                    text: card.entry.id === "map-coverage"
                          ? (typeof translations !== "undefined" ? translations.mapOutOfCoverage : "No map for current location")
                          : card.entry.title || "Notification"
                    color: card.ink
                    font.pixelSize: themeStore.fontBody + 2
                    font.weight: Font.DemiBold
                    maximumLineCount: 2
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 24
                    elide: Text.ElideRight
                }
            }
            Text {
                objectName: "notificationBody"
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: card.entry.id !== "map-coverage" && !!card.entry.body
                text: card.entry.body || ""
                color: card.isDark ? Qt.rgba(1, 1, 1, 0.7) : Qt.rgba(0, 0, 0, 0.87)
                font.pixelSize: themeStore.fontBody
                maximumLineCount: card.compact ? 1 : 2
                wrapMode: Text.WordWrap
                lineHeightMode: Text.FixedHeight
                lineHeight: 22
                elide: Text.ElideRight
            }
        }
        QueuedNotificationCounts {
            queuedCounts: card.queuedCounts
            isDark: card.isDark
        }
    }
}
