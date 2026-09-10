import QtQuick

Row {
    id: counts
    property var queuedCounts: ({})
    property bool isDark: true
    readonly property bool hasCounts: Object.keys(queuedCounts).some(function(key) { return queuedCounts[key] > 0 })
    visible: hasCounts
    height: hasCounts ? 24 : 0
    spacing: 8

    Repeater {
        model: ["error", "warning", "success", "info", "debug"]
        Text {
            required property string modelData
            objectName: "queuedCount_" + modelData
            readonly property int count: counts.queuedCounts[modelData] || 0
            visible: count > 0
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
