import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: card
    property var entry: ({})
    property int criticalCount: 0
    property bool isDark: true
    readonly property color ink: entry.priority === 0 ? (isDark ? "#fff4f3" : "#8f1717")
                                : entry.priority === 2 ? (isDark ? "#ffe0a3" : "#6b4300")
                                : (isDark ? "#f3f6f8" : "#1e2930")
    readonly property color accent: entry.priority === 0 ? (isDark ? "#ff827b" : "#bc2929")
                                   : entry.priority === 2 ? (isDark ? "#ffd075" : "#8a5800")
                                   : (isDark ? "#73c7ff" : "#1976b8")
    implicitHeight: Math.max(72, content.implicitHeight + 16)
    color: entry.priority === 0 ? (isDark ? "#481c20" : "#fee9e7")
         : entry.priority === 2 ? (isDark ? "#382d19" : "#fff1d1")
         : (isDark ? "#202d36" : "#edf4f8")

    Rectangle { width: 3; height: parent.height; color: card.accent }
    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 12
        spacing: 12
        Text {
            objectName: "notificationIcon"
            Layout.preferredWidth: 56
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
                    maxWidth: parent.width
                    text: card.entry.id === "map-coverage"
                          ? (typeof translations !== "undefined" ? translations.mapOutOfCoverage : "No map for current location")
                          : card.entry.title || "Notification"
                    color: card.ink
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    maximumLineCount: 2
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 26
                    elide: Text.ElideRight
                }
            }
            Text {
                objectName: "notificationBody"
                Layout.fillWidth: true
                visible: card.entry.id !== "map-coverage" && !!card.entry.body
                text: card.entry.body || ""
                color: card.isDark ? "#c9d0d6" : "#4b5560"
                font.pixelSize: 20
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                lineHeightMode: Text.FixedHeight
                lineHeight: 24
                elide: Text.ElideRight
            }
        }
        Text {
            visible: card.entry.priority === 0 && card.criticalCount > 1
            text: "+" + (card.criticalCount - 1)
            color: card.ink
            font.pixelSize: 24
            font.weight: Font.DemiBold
        }
    }
}
