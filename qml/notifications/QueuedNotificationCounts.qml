import QtQuick

Item {
    id: counts
    property var queuedCounts: ({})
    property bool isDark: true
    readonly property var kinds: ["error", "warning", "success", "info", "debug"]
    readonly property var visibleKinds: kinds.filter(function(key) { return queuedCounts[key] > 0 })
    readonly property bool hasCounts: visibleKinds.length > 0
    visible: hasCounts
    objectName: "queuedNotificationCounts"
    // Measure independently of the Flow; using its implicit width creates a wrapping loop.
    implicitWidth: hasCounts ? Math.min(112, Math.ceil(countMetrics.advanceWidth) + 5 * visibleKinds.length) : 0
    width: implicitWidth
    implicitHeight: hasCounts ? badges.implicitHeight : 0

    TextMetrics {
        id: countMetrics
        font.pixelSize: 16
        font.weight: Font.DemiBold
        text: counts.visibleKinds.map(function(key) { return "+" + counts.queuedCounts[key] }).join("")
    }

    Flow {
        id: badges
        width: parent.width

        Repeater {
            model: counts.kinds
            Text {
                required property string modelData
                objectName: "queuedCount_" + modelData
                readonly property int count: counts.queuedCounts[modelData] || 0
                visible: count > 0
                width: Math.min(implicitWidth, counts.width)
                rightPadding: 4
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                text: "+" + count
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: modelData === "error" ? (counts.isDark ? "#ff827b" : "#bc2929")
                     : modelData === "warning" ? (counts.isDark ? "#ffd075" : "#8a5800")
                     : modelData === "success" ? (counts.isDark ? "#82dca0" : "#237840")
                     : modelData === "info" ? (counts.isDark ? "#73c7ff" : "#1976b8")
                     : (counts.isDark ? "#b7bdc3" : "#59636c")
                Accessible.name: modelData + ": " + count
            }
        }
    }
}
