import QtQuick
import QtQuick.Layouts
import "../widgets/speedometer"
import "../widgets/status_bars"
import "../widgets/cluster"
import "../widgets/indicators"
import "../widgets/navigation"
import "../widgets/components"
Rectangle {
    id: clusterScreen
    color: themeStore.backgroundColor

    Component.onCompleted: if (typeof bootTimer !== "undefined")
        console.log("[boot +" + bootTimer.elapsed() + "ms] ClusterScreen completed")

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Rare and event-driven — don't hold up the first paint.
            // Confetti only fires on km milestones, TBT is only visible
            // during active navigation. Both incubate asynchronously
            // after the cluster is rendered.
            Loader {
                anchors.fill: parent
                asynchronous: true
                sourceComponent: Component { MilestoneConfettiLayer { anchors.fill: parent } }
            }

            SpeedometerDisplay {
                id: speedometer
                anchors.fill: parent
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                // TBT docks here during navigation; the layout reserves zero
                // height when idle or still incubating. Publishing implicitHeight
                // from the widget removes the need for a sibling spacer to read
                // tbtLoader.item.height.
                Loader {
                    id: tbtLoader
                    Layout.fillWidth: true
                    Layout.preferredHeight: (item && item.visible) ? item.implicitHeight : 0
                    asynchronous: true
                    sourceComponent: Component {
                        TurnByTurnWidget { anchors.fill: parent }
                    }
                }

                BlinkerRow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                }

                Item { Layout.fillHeight: true }

                ClusterBottomBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                }
            }
        }

        UnifiedBottomStatusBar {
            id: bottomBar
            Layout.fillWidth: true
        }
    }

    readonly property real bottomBarHeight: bottomBar.height
}
