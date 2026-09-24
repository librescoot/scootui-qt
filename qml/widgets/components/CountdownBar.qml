import QtQuick

Rectangle {
    id: bar
    property real remainingFraction: 1
    property int transitionMs: 950
    property bool active: true
    implicitHeight: 6
    radius: height / 2
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "#3DFFFFFF" : "#1F000000"

    Rectangle {
        width: parent.width * Math.max(0, Math.min(1, bar.remainingFraction))
        height: parent.height
        radius: parent.radius
        color: "#FF9800"
    }

    Behavior on remainingFraction {
        enabled: bar.active
        NumberAnimation { duration: bar.transitionMs; easing.type: Easing.Linear }
    }
}
