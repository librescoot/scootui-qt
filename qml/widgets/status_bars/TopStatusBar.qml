import QtQuick
import QtQuick.Layouts
import "../indicators"

Rectangle {
    id: topBar
    color: "transparent"
    height: 40

    // --- Degradation coordinator ---------------------------------------
    // Each side publishes levelWidths (its width at every degrade level,
    // computed from TextMetrics + icon constants, independent of the level
    // actually applied). The bar hands each side the highest detail level
    // that fits its budget. With the clock hidden the two sides share one
    // budget and the wider side yields first.
    readonly property bool clockShown: typeof settingsStore === "undefined"
                                       || settingsStore.showClock !== "never"
    readonly property real sideMargin: 8
    readonly property real clockGap: 8

    // Returns the lowest (most detailed) level whose width fits the budget.
    // If none fit, returns the narrowest level rather than blindly the last:
    // levelWidths is not guaranteed monotonic (chipping a short text run, e.g.
    // a 1-2 digit temperature, into a fixed-width chip can widen a level), so
    // the last level is not always the smallest. This only bites the no-fit
    // fallback; when any level fits, the first-fit result is unchanged.
    function minLevelFitting(widths, budget) {
        var best = 0
        for (var i = 0; i < widths.length; i++) {
            if (widths[i] <= budget) return i
            if (widths[i] < widths[best]) best = i
        }
        return best
    }

    readonly property real sideBudget: width / 2 - clockText.width / 2 - clockGap - sideMargin

    readonly property var sharedLevels: {
        var lw = batteryRow.levelWidths || [0]
        var rw = indicatorRow.levelWidths || [0]
        var budget = width - 2 * sideMargin - 12
        var l = 0, r = 0
        while (lw[l] + rw[r] > budget) {
            var lCan = l < lw.length - 1
            var rCan = r < rw.length - 1
            if (!lCan && !rCan) break
            if (lCan && (!rCan || lw[l] >= rw[r])) l++
            else r++
        }
        return [l, r]
    }

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] TopStatusBar completed")

    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: themeStore.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 0

        // Left: Battery display
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 2

            BatteryDisplay {
                id: batteryRow
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                degradeLevel: topBar.clockShown
                              ? topBar.minLevelFitting(levelWidths, topBar.sideBudget)
                              : topBar.sharedLevels[0]
            }
        }

        // Center: Clock
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            visible: typeof settingsStore === "undefined" || settingsStore.showClock !== "never"

            Text {
                id: clockText
                anchors.centerIn: parent
                font.pixelSize: themeStore.fontTitle
                font.weight: Font.Medium
                color: themeStore.textColor

                readonly property string overrideStr:
                    typeof simulator !== "undefined" ? simulator.clockOverride : ""

                property string timeStr: ""
                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: {
                        var now = new Date()
                        var h = now.getHours().toString().padStart(2, '0')
                        var m = now.getMinutes().toString().padStart(2, '0')
                        clockText.timeStr = h + ":" + m
                    }
                }
                text: overrideStr.length > 0 ? overrideStr : timeStr
            }
        }

        // Right: Status indicators
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 2

            StatusIndicators {
                id: indicatorRow
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                degradeLevel: topBar.clockShown
                              ? topBar.minLevelFitting(levelWidths, topBar.sideBudget)
                              : topBar.sharedLevels[1]
            }
        }
    }
}
