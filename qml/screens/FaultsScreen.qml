import QtQuick
import QtQuick.Layouts
import "../widgets/components"

Rectangle {
    id: faultsScreen
    color: typeof themeStore !== "undefined" && themeStore.isDark ? "black" : "white"

    readonly property bool isDark: typeof themeStore !== "undefined" ? themeStore.isDark : true
    readonly property color textPrimary: isDark ? "#FFFFFF" : "#000000"
    readonly property color textSecondary: isDark ? "#99FFFFFF" : "#8A000000"
    readonly property color textTertiary: isDark ? "#66FFFFFF" : "#5A000000"
    readonly property color dividerColor: isDark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color criticalColor: isDark ? "#FF5252" : "#B00020"
    readonly property color warningColor: isDark ? "#FFB300" : "#BF6500"
    readonly property color activeBg: isDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)

    // Periodic tick so "(20h ago)" strings refresh without a timestamp changing.
    property real nowMs: Date.now()
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: faultsScreen.nowMs = Date.now()
    }

    function formatRelative(ts) {
        if (!ts || ts <= 0)
            return ""
        var diff = faultsScreen.nowMs - ts
        if (diff < 60000) return "just now"
        if (diff < 3600000) return Math.floor(diff / 60000) + "m ago"
        if (diff < 86400000) return Math.floor(diff / 3600000) + "h ago"
        return Math.floor(diff / 86400000) + "d ago"
    }

    function formatIso(ts) {
        if (!ts || ts <= 0)
            return ""
        var d = new Date(ts)
        function p(n) { return n < 10 ? "0" + n : "" + n }
        return d.getFullYear() + "-" + p(d.getMonth() + 1) + "-" + p(d.getDate())
             + " " + p(d.getHours()) + ":" + p(d.getMinutes())
    }

    function formatStamp(ts) {
        if (!ts || ts <= 0)
            return ""
        return formatIso(ts) + " (" + formatRelative(ts) + ")"
    }

    function closeScreen() {
        if (typeof screenStore !== "undefined")
            screenStore.closeFaults()
    }

    readonly property var entries: typeof faultsStore !== "undefined" ? faultsStore.entries : []

    Connections {
        target: typeof inputHandler !== "undefined" ? inputHandler : null
        function onLeftTap() {
            var maxY = Math.max(0, flickable.contentHeight - flickable.height)
            scrollAnim.to = Math.min(flickable.contentY + 120, maxY)
            scrollAnim.restart()
        }
        function onLeftHold() {
            scrollAnim.to = Math.max(flickable.contentY - 120, 0)
            scrollAnim.restart()
        }
        function onRightTap() { faultsScreen.closeScreen() }
    }

    Column {
        anchors.fill: parent

        // Header
        Item {
            id: header
            width: parent.width
            height: 56

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: typeof translations !== "undefined" ? translations.menuFaults : "Faults"
                color: faultsScreen.textPrimary
                font.pixelSize: themeStore.fontBody + 6
                font.weight: Font.DemiBold
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    var active = typeof faultsStore !== "undefined" ? faultsStore.activeCount : 0
                    var total = faultsScreen.entries.length
                    if (active > 0)
                        return active + " active / " + total + " total"
                    return total + " total"
                }
                color: faultsScreen.textSecondary
                font.pixelSize: themeStore.fontCaption
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: faultsScreen.dividerColor
            }
        }

        // List
        Flickable {
            id: flickable
            width: parent.width
            height: parent.height - header.height - controlBar.height
            contentHeight: content.height
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
                id: content
                width: flickable.width
                spacing: 0

                Repeater {
                    model: faultsScreen.entries

                    delegate: Rectangle {
                        width: content.width
                        color: modelData.active ? faultsScreen.activeBg : "transparent"
                        height: rowContent.height + 20

                        readonly property color sevColor: modelData.severity === 0
                            ? faultsScreen.criticalColor : faultsScreen.warningColor
                        readonly property real textAlpha: modelData.active ? 1.0 : 0.7

                        Column {
                            id: rowContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 4

                            // First line: severity icon, source badge, code, count
                            Row {
                                spacing: 8

                                Text {
                                    text: MaterialIcon.iconWarningAmber
                                    font.family: "Material Icons"
                                    font.pixelSize: themeStore.fontBody
                                    color: parent.parent.parent.sevColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                // Source badge
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: srcLabel.width + 12
                                    height: srcLabel.height + 6
                                    color: faultsScreen.dividerColor
                                    radius: 4
                                    Text {
                                        id: srcLabel
                                        anchors.centerIn: parent
                                        text: modelData.sourceLabel
                                        color: faultsScreen.textPrimary
                                        opacity: parent.parent.parent.parent.textAlpha
                                        font.pixelSize: themeStore.fontCaption
                                        font.weight: Font.DemiBold
                                        font.letterSpacing: 0.5
                                    }
                                }

                                // Code label
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.codeLabel
                                    color: parent.parent.parent.sevColor
                                    opacity: parent.parent.parent.textAlpha
                                    font.pixelSize: themeStore.fontBody
                                    font.weight: Font.Bold
                                    font.family: "monospace"
                                }

                                // Count (only if > 1)
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: modelData.raiseCount > 1
                                    text: "×" + modelData.raiseCount
                                    color: faultsScreen.textSecondary
                                    opacity: parent.parent.parent.textAlpha
                                    font.pixelSize: themeStore.fontCaption
                                }

                                // Spacer pushing active/cleared badge to right
                                Item { height: 1; width: 8 }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.active
                                        ? (typeof translations !== "undefined" ? translations.faultActive : "active")
                                        : (typeof translations !== "undefined" ? translations.faultCleared : "cleared")
                                    color: modelData.active ? parent.parent.parent.sevColor : faultsScreen.textTertiary
                                    font.pixelSize: themeStore.fontCaption
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 0.5
                                }
                            }

                            // Description
                            Text {
                                width: parent.width
                                visible: modelData.description && modelData.description.length > 0
                                text: modelData.description || ""
                                color: faultsScreen.textPrimary
                                opacity: parent.parent.textAlpha
                                font.pixelSize: themeStore.fontBody
                                wrapMode: Text.WordWrap
                            }

                            // Timestamps
                            Text {
                                width: parent.width
                                color: faultsScreen.textSecondary
                                opacity: parent.parent.textAlpha
                                font.pixelSize: themeStore.fontMicro
                                font.family: "monospace"
                                wrapMode: Text.WordWrap
                                text: {
                                    if (modelData.active) {
                                        // Active: "since <first>" when known, otherwise nothing.
                                        if (modelData.firstRaisedMs > 0)
                                            return "since " + faultsScreen.formatStamp(modelData.firstRaisedMs)
                                        return ""
                                    }
                                    // History: first / last raised / cleared.
                                    var parts = []
                                    if (modelData.firstRaisedMs > 0)
                                        parts.push("first " + faultsScreen.formatStamp(modelData.firstRaisedMs))
                                    if (modelData.lastRaisedMs > 0
                                        && modelData.lastRaisedMs !== modelData.firstRaisedMs)
                                        parts.push("last " + faultsScreen.formatStamp(modelData.lastRaisedMs))
                                    if (modelData.clearedAtMs > 0)
                                        parts.push("cleared " + faultsScreen.formatStamp(modelData.clearedAtMs))
                                    return parts.join("  ·  ")
                                }
                                visible: text !== ""
                            }
                        }

                        // Row divider
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: faultsScreen.dividerColor
                        }
                    }
                }

                // Empty state
                Item {
                    width: content.width
                    height: 160
                    visible: faultsScreen.entries.length === 0

                    Text {
                        anchors.centerIn: parent
                        text: typeof translations !== "undefined" ? translations.faultsEmpty : "No faults recorded"
                        color: faultsScreen.textSecondary
                        font.pixelSize: themeStore.fontBody
                    }
                }

                Item { width: 1; height: 24 }
            }
        }

        // Footer
        Rectangle {
            id: controlBar
            width: parent.width
            height: controlHints.height
            color: faultsScreen.isDark ? "black" : "white"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: faultsScreen.dividerColor
            }

            ControlHints {
                id: controlHints
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                leftAction: typeof translations !== "undefined"
                            ? translations.aboutScrollAction : "Scroll"
                rightAction: typeof translations !== "undefined"
                             ? translations.aboutBackAction : "Back"
            }
        }
    }
}
